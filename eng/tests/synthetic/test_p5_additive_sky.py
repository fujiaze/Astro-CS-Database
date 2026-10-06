#!/usr/bin/env python3
"""P5 加性天光去除与无接缝叠加 —— 合成全链层的接缝判据与加性校正不变量。

**创新点**：`AGENTS.md` §10「P5 加性天光去除 | 信噪比加权去除加性天光、保留公共背景，
叠加天然无接缝」。

**为什么这一层是空白**（只读审计结论）：`grep -rln 'seam|rel_step|天光'` 在
unit/module/integration 三层只命中三个**辅助模块**，**无一条判据**。⇒ 本文件是该覆盖的
第一批。P5 的正本材料最厚（`PHASE2_UPM.md` §17 是全仓少有的**完整推导链**），
所以本文件的重心放在「接缝门槛 `1e-2` 的推导链逐环可复算」上。

## 0 判据恒真清单（如实登记，含本文件**判定恒真因而没写成用例**的）

| 候选判据 | 判定 | 依据 |
|---|---|---|
| 「跨帧平方和 `Σ SNR_k² = F0²/Var(F̂)`」 | ❌ 恒真（定义式） | `NOISE_SNR.md:353-359` 逐字「结构上不可能给出判决」。黑名单，本文件不写 |
| 「把整张背景减掉后再比帧间差」 | ❌ 退化 | `GATES_AND_TOLERANCES.md` §接缝与 WCS 基线逐字；本文件 P5-c 负例即此 |
| 「在 8×8 control cell 的**控制点**处测加性校正残差」 | ❌ 恒真 | 双线性在控制点上恒等于输入，残差是**构造定义**；本文件 P5-f 负例即此 |
| 「rel_step ≤ 1e-2」在**无噪声**臂上 | ❌ 恒绿 | 噪声下界面上判据量由噪声决定；无噪声臂只用于闭式对拍与确定性硬界 |
| 「质量比 `E`」判重建/权重水平偏差 | ❌ 对整体乘性偏差免疫 | `NOISE_SNR.md:484-492`；本文件不用 `E` |
| 候选判据 `Δ̂ = (4·s_d − s_4d)/3` 当门 | ❌ 越权 | `PHASE2_UPM.md` §17.3 第 6 条逐字「**尚未进入判决面**：判据面变更须走变更流程」；本文件只作诊断量 |

## 1 本文件最硬的一条纪律：`σ_seam` 必须取实测

`PHASE2_UPM.md` §17.1 逐字：「**`σ_seam` 必须取实测**：`σ_seam = √2·σ_pix` 只在 ±d 采样
互不相关时成立；d = 2 px 的采样点多落在亚像素位置，双线性插值会再压一次方差
（各向异性权重 ⇒ 单样本方差因子 ≈ 4/9）……**解析式高估 1.59 倍** ⇒ 一律用工具落盘的
`seam_mad`。」⇒ 本文件里 `√2σ_pix` 只出现在**被否定的对照臂**里（P5-a 负例）。

## 2 夹具来源（逐字照 §17.3）

`PHASE2_UPM.md` §17.3 合成夹具实测行逐字：「噪声取到与 M42 **同量级**：
`σ_pix/bg = 1.0703e-2`；**4 条边**受影响、**30 seed**/点」。本文件逐字复用这三个数。
`d = 2 px`、`N_e = 256` 取 §17.1 逐字默认值。
"""

from __future__ import annotations

import math
import os
import sys

import numpy as np

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

from eng.tests.synthetic import _tol_chain_b as TB          # noqa: E402
from eng.tests.synthetic import tolerances as _base_tol     # noqa: E402
from eng.tests.unit import harness as H                     # noqa: E402


# ===========================================================================
# §A 接缝判据（`PHASE2_UPM.md` §17.1 逐字）
# ===========================================================================

GATE = TB.get("synth.p5.seam_gate").value          # 1.0e-2（正本逐字）
D_NORM = 2.0                                       # §17.1「d = 采样半距（默认 2 px）」
N_E = 256                                          # §17.1「N_e … 默认 256 点」
CTRL_SHIFT = 200.0                                 # --ctrl-shift 默认（适用域门）
MAD_K = 1.482602218505602                          # §17.1「MAD = 1.482602218505602×中位绝对偏差」
SQRT_HALF_PI = math.sqrt(math.pi / 2.0)            # §17.1「√(π/2) = 1.2533141…」


def bilinear_sample(img: np.ndarray, sx: np.ndarray, sy: np.ndarray) -> np.ndarray:
    """亚像素双线性采样（§17.1 逐字「采样点多落在亚像素位置，双线性插值会再压一次方差」）。"""
    h, w = img.shape
    sx = np.clip(sx, 0.0, w - 1.0000001)
    sy = np.clip(sy, 0.0, h - 1.0000001)
    i = np.floor(sx).astype(int)
    j = np.floor(sy).astype(int)
    tx, ty = sx - i, sy - j
    return (img[j, i] * (1 - tx) * (1 - ty) + img[j, i + 1] * tx * (1 - ty)
            + img[j + 1, i] * (1 - tx) * ty + img[j + 1, i + 1] * tx * ty)


def footprint_edges(box):
    """帧足迹的四条边界（`§17.4` 逐字「顺序固定为 bottom / top / left / right」）。

    每条返回 `(name, 采样点 Nx2, 指向画幅内部的单位法向)`。
    """
    y0, y1, x0, x1 = box
    t = np.linspace(0.0, 1.0, N_E)
    return [
        ("bottom", np.stack([x0 + t * (x1 - x0 - 1), np.full(N_E, float(y0))], axis=1),
         np.array([0.0, 1.0])),
        ("top", np.stack([x0 + t * (x1 - x0 - 1), np.full(N_E, float(y1))], axis=1),
         np.array([0.0, -1.0])),
        ("left", np.stack([np.full(N_E, float(x0)), y0 + t * (y1 - y0 - 1)], axis=1),
         np.array([1.0, 0.0])),
        ("right", np.stack([np.full(N_E, float(x1)), y0 + t * (y1 - y0 - 1)], axis=1),
         np.array([-1.0, 0.0])),
    ]


def edge_metric(img: np.ndarray, pts: np.ndarray, normal: np.ndarray,
                d: float = D_NORM) -> dict:
    """单条边界的接缝判据量与噪声诊断量（§17.1 逐字）。

        rel_step(e) = median_i seam_i(e) / bg(e)         ← **有符号**
        seam_i(e)   = I(p_i + d·n_e) − I(p_i − d·n_e),  i = 1..N_e
        bg(e)       = median(|I| over the 2·N_e ±d 采样点)
    """
    smp = {s: bilinear_sample(img, pts[:, 0] + s * d * normal[0],
                              pts[:, 1] + s * d * normal[1]) for s in (+1, -1)}
    seam = smp[+1] - smp[-1]
    bg = float(np.median(np.abs(np.concatenate([smp[+1], smp[-1]]))))
    seam_med = float(np.median(seam))
    return {
        "rel_step": seam_med / bg,
        "bg": bg,
        "seam_mad": MAD_K * float(np.median(np.abs(seam - seam_med))),
        "seam_med": seam_med,
        "sigma_rel_measured": SQRT_HALF_PI * (MAD_K * float(np.median(np.abs(seam - seam_med))))
                              / (math.sqrt(N_E) * bg),
    }


def seam_readings(img: np.ndarray, box, d: float = D_NORM) -> dict:
    return {name: edge_metric(img, pts, n, d) for name, pts, n in footprint_edges(box)}


def max_abs_rel_step(img: np.ndarray, box, d: float = D_NORM) -> float:
    """判决量：`max_e |rel_step(e)|`（§17.3 逐字「判决量是 `max_e |rel_step(e)|`」）。"""
    return max(abs(v["rel_step"]) for v in seam_readings(img, box, d).values())


# -- 夹具 -------------------------------------------------------------------

LEVEL = 1000.0                     # 边界一侧局部电平 L（ADU/px，冻结字面常量）
BOX = (120, 392, 120, 392)         # 512×512 画幅内的帧足迹；两侧都够放 ±ctrl_shift
SIGMA_PIX_REL = 1.0703e-2          # §17.3 逐字「σ_pix/bg = 1.0703e-2」
N_SEEDS = 30                       # §17.3 逐字「30 seed/点」
SEED0 = 20260928                   # 定种子（TEST.md §3「固定种子」）


def step_scene(amp: float, seed: int | None = None, box=BOX, level: float = LEVEL,
               shape=(512, 512), sigma_pix_rel: float = SIGMA_PIX_REL):
    """帧足迹内比外侧高 `amp` 的场景（物理电平台阶 Δ，`bg` 实测 = L + Δ/2）。"""
    h, w = shape
    y0, y1, x0, x1 = box
    img = np.full(shape, float(level))
    img[y0:y1, x0:x1] += amp
    if seed is not None:
        rng = np.random.default_rng(seed)
        img = img + rng.normal(0.0, sigma_pix_rel * level, size=shape)
    return img


def ramp_scene(rho: float, level: float = LEVEL, shape=(512, 512)):
    """**指数**斜坡：`I(x) = level·exp(ρ(x − x_mid))`，使 ρ 恰为**每像素相对法向斜率**。

    ⚠ 线性斜坡会让 ρ 随电平漂移，闭式 `rel_step = 2dρ` 随之不成立——那是夹具错误。
    §17.3 第 6 条逐字要求「ρ（按边界处局部电平归一）」，指数型是它的字面落法。
    """
    h, w = shape
    _, xx = np.mgrid[0:h, 0:w]
    return level * np.exp(rho * (xx - w / 2.0))


# ===========================================================================
# §B 加性校正（`PHASE2_UPM.md` §5）与权重（§5 两级权重）
# ===========================================================================


def bilinear_control_field(theta: np.ndarray, xs: np.ndarray, ys: np.ndarray) -> np.ndarray:
    """`C_f(p) = 双线性(8×8 control cell, θ_f)`（§5 逐字）。

    `theta` 的节点落在 control cell **中心**，与 `weight_chain.h` 的 cell 中心约定同源。
    """
    ny, nx = theta.shape
    # 节点落在所属 control cell **中心**（与 `weight_chain.h` 的 cell 中心约定同源）：
    # 归一化坐标 x 的第 i 个节点在 (i + 0.5)/nx ⇒ 节点索引坐标 u = nx·x − 0.5。
    u = np.asarray(xs, float) * nx - 0.5
    v = np.asarray(ys, float) * ny - 0.5
    i = np.clip(np.floor(u).astype(int), 0, nx - 2)
    j = np.clip(np.floor(v).astype(int), 0, ny - 2)
    tu = np.clip(u - i, 0.0, 1.0)
    tv = np.clip(v - j, 0.0, 1.0)
    return (theta[j, i] * (1 - tu) * (1 - tv) + theta[j, i + 1] * tu * (1 - tv)
            + theta[j + 1, i] * (1 - tu) * tv + theta[j + 1, i + 1] * tu * tv)


def true_sky_field(shape, level: float = LEVEL):
    """真天光面：均匀电平 + 可被 8×8 网格**表示但不被双线性精确表示**的二次项。

    `f(x,y) = level·(1 + 2.0e-2·x + 1.0e-2·y + 3.0e-2·x² + 1.0e-2·xy)`（x,y 归一化到 [0,1]）。
    二次项是**关键**：它让真面与双线性插值面之差**非零**，从而残差判据不恒真；
    若真面是仿射的，双线性精确重建 ⇒ 残差恒 0 ⇒ 判据恒绿。
    """
    h, w = shape
    j, i = np.meshgrid(np.linspace(0, 1, h), np.linspace(0, 1, w), indexing="ij")
    return level * (1.0 + 2.0e-2 * i + 1.0e-2 * j + 3.0e-2 * i ** 2 + 1.0e-2 * i * j)


def solve_additive_gauge(raw_by_frame, weights, ref_index: int = 0):
    """加性面的 gauge 固定求解（§5 逐字「每分量参考帧 = 最小 `frame_id`」⇒ `C_ref = 0`）。

    未知量：公共面 `M`（per control）+ 逐帧平面 `C_f`（per control），约束 `C_ref = 0`。
    最小二乘：`Σ_f Σ_c w_f (raw_{f,c} − M_c − C_{f,c})²`。
    """
    n_frames = len(raw_by_frame)
    n_ctrl = raw_by_frame[0].size
    nf = n_frames * n_ctrl
    nm = n_ctrl
    # 参数 x = [C_1..C_{n_frames} (ref 被约束掉) ; M_1..M_{n_ctrl}]
    free = [f for f in range(n_frames) if f != ref_index]
    n_par = len(free) * n_ctrl + nm
    a = np.zeros((nf, n_par))
    b = np.zeros(nf)
    for row, f in enumerate(range(n_frames)):
        for c in range(n_ctrl):
            i = row * n_ctrl + c
            # ⚠ 参考帧的 gauge 是 `C_ref ≡ 0`：它的方程里**不得**出现 C 块列，
            # 否则设计矩阵在每个 control 上都退化成秩 1（lstsq 给 min-norm 解，
            # 把公共常数劈成 M 与 C_f 两半 —— 正是本条要抓的错误读数）。
            if f in free:
                a[i, free.index(f) * n_ctrl + c] = 1.0
            a[i, len(free) * n_ctrl + c] = 1.0
            b[i] = raw_by_frame[f].ravel()[c]
    w = np.concatenate([np.asarray(weights[f]).ravel() for f in range(n_frames)])
    sw = np.sqrt(np.abs(w))
    x, *_ = np.linalg.lstsq(a * sw[:, None], b * sw, rcond=None)
    c_planes = np.zeros((n_frames, n_ctrl))
    for f2 in free:
        c_planes[f2] = x[free.index(f2) * n_ctrl:(free.index(f2) + 1) * n_ctrl]
    return x[len(free) * n_ctrl:], c_planes


def w_upm(quality_factor, control_reliability, control_ivar) -> np.ndarray:
    """绝对式 `w_UPM = quality_factor × control_reliability × control_ivar`（§5 逐字）。"""
    return quality_factor * control_reliability * control_ivar


def w_cell(w_upm_values, control_reliability) -> np.ndarray:
    """份额式 `w_cell = w_UPM / Σ_cell w_UPM × control_reliability`（§5 逐字）。"""
    s = np.sum(w_upm_values)
    return w_upm_values / s * control_reliability


def huber_loss(z: np.ndarray | float, delta: float) -> np.ndarray:
    """Huber 损失，**作用于无量纲 z**：小残差 `0.5z²`、大残差 `δ|z| − 0.5δ²`（§7 逐字）。"""
    z = np.asarray(z, float)
    a = np.abs(z)
    return np.where(a <= delta, 0.5 * z ** 2, delta * a - 0.5 * delta ** 2)


# ===========================================================================
# §C P5-a 有符号电平台阶检出
# ===========================================================================


@H.test(
    "p5a_signed_step_detection_closed_form",
    intent="注入已知 `Δ` 后判据量与闭式 `rel_step = Δ/(L + Δ/2)` 逐位吻合；"
           "随 Δ/L **单调上升**；并在确定性下限 `Δ/L > 1.0050%` 处必判红、其下必判绿",
    inputs="512×512 画幅、足迹 272×272 贴内、四条边各 N_e = 256 采样点、d = 2 px；"
           "L = 1000 ADU/px；Δ 从 0 扫到 2%·L（**无噪声**臂，判据量此时是闭式）",
    expected="① 每档 `|rel_step − Δ/(L+Δ/2)| / 闭式` ≤ 1.0e-12"
             "（synth.p5.rel_step_closed_rel）；② 逐档 `max_e|rel_step|` 严格单调上升；"
             "③ `Δ/L = 0.99 × 1.0050%` 判绿、`1.0050% × 1.005` 判红"
             "（synth.p5.seam_step_crit_rel 闭式）",
    source="闭式解析（`PHASE2_UPM.md` §17.3 逐字「确定性下限（无噪声硬界）："
           "`rel_step = Δ/(L + Δ/2)`（L = 边界一侧局部电平；`bg` 实测 = L + Δ/2）"
           "⇒ `Δ/L > gate/(1 − gate/2) = 1.0050%`（约 0.0109 mag）**必判红**」）；"
           "判据量定义同 §17.1 逐字；门 `1e-2` 取 §9a 逐字",
)
def p5a_signed_step_detection_closed_form():
    tol_closed = TB.get("synth.p5.rel_step_closed_rel").value
    crit = TB.get("synth.p5.seam_step_crit_rel").value
    amp_crit = crit * LEVEL / (1.0 - crit / 2.0)
    fracs = [0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0]
    with H.evidence() as ev:
        ev.record("gate", GATE, 0.01, "", "§9a 逐字门值")
        ev.record("deterministic_critical_Delta_over_L", crit, 0.0100502512562814,
                  "", "闭式 gate/(1−gate/2)")
        prev = -1.0
        for fr in fracs:
            amp = fr * LEVEL
            img = step_scene(amp)
            got = max_abs_rel_step(img, BOX)
            closed = amp / (LEVEL + amp / 2.0)
            rel = abs(got - closed) / closed if closed > 0 else abs(got)
            ev.record(f"Delta/L={fr:.2%}", got, closed, "",
                      f"闭式对照；相对偏差 = {rel:.2e}")
            H.close(got, closed, rtol=tol_closed, atol=0.0,
                    what=f"Δ/L={fr:.4f}: rel_step 对闭式")
            H.is_true(got > prev, f"Δ/L={fr:.4f}: 判据量未严格单调上升")
            prev = got
        # 确定性硬界两侧
        for mult, expect_red in ((0.99, False), (1.005, True), (1.10, True)):
            amp = mult * amp_crit
            got = max_abs_rel_step(step_scene(amp), BOX)
            ev.record(f"critical x {mult:.3f} (Delta/L={amp/LEVEL:.5%})", got, GATE, "",
                      "确定性硬界两侧")
            H.exact(got > GATE, expect_red,
                    f"Δ/L = {mult:.3f}×临界处的判决")


@H.test(
    "p5a_negative_analytic_sigma_seam_shortcut",
    intent="**注入被正本禁止的捷径**：`σ_seam` 取 `√2·σ_pix` 而不是实测 `MAD(seam)`；"
           "解析式**高估**噪声 ⇒ 用它推出的可检出下限被高估",
    inputs="与 P5-a 同一带噪夹具（σ_pix/bg = 1.0703e-2、4 条边、30 seed）；"
           "对照实现把 `seam_mad` 换成 `√2·σ_pix`（只影响 σ 诊断与检出下限外推，"
           "不影响 rel_step 本身）",
    expected="逐条边 `σ_rel(√2σ_pix) / σ_rel(实测) ≥ 1.0`（synth.p5."
             "sigma_seam_analytic_over_min_min）；且解析式推出的 50% 检出 `Δ/L` "
             "**大于**按实测 σ 推出的值 ⇒ 门承诺的检出下限被高估",
    source="正本 `PHASE2_UPM.md` §17.1 逐字「**`σ_seam` 必须取实测**……"
           "**解析式高估 1.59 倍** ⇒ 一律用工具落盘的 `seam_mad`」；"
           "§17.3 逐字 `P_detect = 1 − Π_e Φ((gate−μ_e)/σ_e)`（σ 直接进检出下限）",
    kind=H.NEGATIVE,
    inject="把噪声尺度从实测 `1.482602218505602·median|seam − median(seam)|` 换成解析式 "
           "`√2·σ_pix`——这是「按白噪声独立采样推导」时最顺手的写法",
    defect_id="P5-A-ANALYTIC-SIGMA-SEAM",
)
def p5a_negative_analytic_sigma_seam_shortcut():
    tol = TB.get("synth.p5.sigma_seam_analytic_over_min").value
    ratios, sigma_meas, sigma_anal = [], [], []
    for seed in range(SEED0, SEED0 + N_SEEDS):
        img = step_scene(0.0, seed=seed)          # 无台阶 ⇒ 纯 H0，读 σ 诊断
        for name, v in seam_readings(img, BOX).items():
            bg = v["bg"]
            s_meas = v["sigma_rel_measured"]
            s_anal = SQRT_HALF_PI * (math.sqrt(2.0) * SIGMA_PIX_REL * LEVEL) \
                     / (math.sqrt(N_E) * bg)
            ratios.append(s_anal / s_meas)
            sigma_meas.append(s_meas)
            sigma_anal.append(s_anal)
    lo = min(ratios)
    # 50% 检出下限：Φ((gate − μ)/σ) = 0.5 ⇒ μ = gate ⇒ Δ/L = gate/(1 − gate/2)，
    # 在「σ 取实测」与「σ 取解析」两套口径下分别用各自的 σ 折算所需 Δ/L：
    #   统计下限相对确定性下限的抬升 = z_{0.5..} 无关；这里用 §17.3 的口径：
    #   需 μ_e − gate ≥ 0 时才必红；50% 检出对应 μ_e = gate + 0（σ 只决定散布）。
    # ⇒ 改用**更直接可判**的量：解析 σ 给出的散布比实测散布大多少 ⇒ 同一 Δ/L 下
    #   解析口径预测的检出率显著低于实测检出率。
    spread_ratio = float(np.mean(sigma_anal) / np.mean(sigma_meas))
    with H.evidence() as ev:
        ev.record("min_edge analytic/measured sigma_rel", lo, tol, "",
                  f"4 条边 × {N_SEEDS} seed = {len(ratios)} 条读数；"
                  f"中位 {np.median(ratios):.4f}、最大 {max(ratios):.4f}")
        ev.record("mean_edge analytic/measured sigma_rel", spread_ratio, tol, "",
                  f"正本 §17.1 在其夹具上读到 1.59×；本夹具 1.05×–{max(ratios):.2f}×")
        H.is_true(lo >= tol,
                  f"解析 σ_seam 在某条边上**低估**了噪声（比值 {lo:.4f} < 1）⇒ 注入失效")
        H.is_true(spread_ratio > 1.0,
                  "解析口径的 σ 与实测口径同量级 ⇒ 注入未产生可观测后果")
        # 后果量化。⚠ 取样点必须在**确定性下限之下**：`Δ/L = 临界` 时
        # `μ = Δ/(L+Δ/2) = gate` 恒成立 ⇒ `Φ((gate−μ)/σ) ≡ Φ(0) = 0.5` 与 σ 无关，
        # 在那里比检出率是**恒真**的（派单纪律 §3.2 型 3 的同族陷阱）。
        # 正本 §17.3 的合成夹具点位 0.90%/0.95%/1.00% 正是为此选的。
        def p_detect(delta_rel, sigma_rel):
            mu = delta_rel / (1.0 + delta_rel / 2.0)
            return 0.5 * (1.0 + math.erf((GATE - mu) / sigma_rel / math.sqrt(2.0)))
        meas_s = float(np.mean(sigma_meas))
        anal_s = float(np.mean(sigma_anal))
        d_probe = 0.0090                      # 正本 §17.3 合成夹具点位（< 1.0050% 临界）
        p_m = p_detect(d_probe, meas_s)
        p_a = p_detect(d_probe, anal_s)
        ev.record("P_detect(Delta/L=0.90%) with measured sigma", p_m, None, "",
                  "同一 Δ/L 下两套 σ 口径给出的单边检出概率")
        ev.record("P_detect(Delta/L=0.90%) with analytic sigma", p_a, None, "",
                  "解析式高估 σ ⇒ 预测的检出率被压低")
        ev.record("P_detect ratio (analytic/measured)", p_a / p_m, 1.0, "",
                  "< 1 ⇒ 解析口径**低估**检出率 ⇒ 门承诺的检出下限被高估")
        ev.record("spurious-point check: P_detect at the critical Delta/L",
                  p_detect(TB.get("synth.p5.seam_step_crit_rel").value, meas_s),
                  p_detect(TB.get("synth.p5.seam_step_crit_rel").value, anal_s), "",
                  "⚠ 两值恒为 0.5（μ = gate）⇒ 在临界点上比检出率是恒真的，已避开")
        H.is_true(p_a < 0.95 * p_m,
                  f"解析 σ 未使检出率预测实质变保守（实测比值 {p_a / p_m:.4f}）⇒ "
                  "注入无后果，需如实登记")


# ===========================================================================
# §D P5-b 判据非恒真（正本逐字给出的现成证明）
# ===========================================================================


@H.test(
    "p5b_zero_step_zero_ramp_is_green",
    intent="**判据非恒真的现成证明**（正本逐字）：`ρ = 0` 且 `Δ = 0` 时 `rel_step ≡ 0`，"
           "4000 次 MC 全绿",
    inputs="无噪声夹具（正本 §17.3 第 6 条证据行逐字「负例 ρ = 0 ⇒ rel_step 恒 0、"
           "4000 次 MC 全绿 ⇒ 判据非恒真」）；同一 L、四条边、N_e = 256、d = 2 px",
    expected="4000 次重复的 `max_e|rel_step|` **全部为 0.0**（逐位，一个不同的取值都没有）"
             "⇒ 判据在同一夹具下能绿，且 P5-a 的红项来自注入而不是基线",
    source="正本条款 + 闭式：`PHASE2_UPM.md` §17.3 第 6 条证据行逐字"
           "「20 项断言全绿；负例 ρ = 0 ⇒ rel_step 恒 0、**4000 次 MC 全绿** ⇒ 判据非恒真」；"
           "`§17.3` 逐字「**注入已知台阶（`amp ≠ 0`）必须判红、`amp = 0` 必须回落到基线**"
           "（`§9a` 判据必须**能红能绿**）」",
)
def p5b_zero_step_zero_ramp_is_green():
    vals = set()
    worst = 0.0
    for i in range(4000):
        worst = max(worst, max_abs_rel_step(step_scene(0.0), BOX))
        vals.add(worst)
    with H.evidence() as ev:
        ev.record("replicates", 4000.0, 4000.0, "", "正本 §17.3 的 4000 次 MC")
        ev.record("max|rel_step| over 4000 reps", worst, 0.0, "",
                  "恒零 ⇒ 判据在该夹具上全绿")
        ev.record("distinct values observed", float(len(vals)), 1.0, "",
                  "逐位恒零，不是「小于某个容差」")
        H.exact(worst, 0.0, "无台阶无斜坡时的判据量")
        H.exact(len(vals), 1, "4000 次重复上判据量的不同取值个数")
        # 同时登记带噪基线：噪声单独造成的判据量水位（P5-a 的「基线」读数）
        noisy = [max_abs_rel_step(step_scene(0.0, seed=SEED0 + s), BOX)
                 for s in range(N_SEEDS)]
        ev.record("noisy baseline median (amp=0, 30 seed)",
                  float(np.median(noisy)), GATE, "",
                  "amp=0 臂必须回落到基线（低于门）")
        H.is_true(float(np.median(noisy)) < GATE,
                  "amp = 0 臂未回落到基线（判据不能红能绿）")


# ===========================================================================
# §E P5-c 保留公共背景的可判定性（退化判据负例）
# ===========================================================================


@H.test(
    "p5c_keep_background_is_decidable",
    intent="**保留公共天光面 `B_ref`** 的口径下判据**有判别力**："
           "`Δ/L` 从 0 扫到 6%，判红率从 0/30 单调升到 30/30",
    inputs="同一夹具（σ_pix/bg = 1.0703e-2、4 条边、30 seed），"
           "`Δ/L ∈ {0, 0.5%, 1.0%, 3.0%, 6.0%}`",
    expected="判红率随 `Δ/L` 单调不降；`Δ/L = 0` 时 0/30 判红（回落基线）、"
             "`Δ/L ≥ 3%` 时 30/30 判红 ⇒ 能红能绿",
    source="正本 `GATES_AND_TOLERANCES.md` §接缝与 WCS 基线逐字「**接缝判据**：在"
           "**保留公共天光面 `B_ref`** 的前提下比较帧间一致性与边界跳变……"
           "判据只用保留 `B_ref` 的写法」；`PHASE2_UPM.md` §9a 逐字「判据必须**能红能绿**」",
)
def p5c_keep_background_is_decidable():
    rates = []
    with H.evidence() as ev:
        prev = -1
        for dfrac in (0.0, 0.005, 0.010, 0.030, 0.060):
            amp = dfrac * LEVEL
            reds = 0
            mags = []
            for s in range(N_SEEDS):
                m = max_abs_rel_step(step_scene(amp, seed=SEED0 + s), BOX)
                mags.append(m)
                reds += int(m > GATE)
            rates.append((dfrac, reds))
            ev.record(f"Delta/L={dfrac:.2%} 判红率", reds / N_SEEDS, GATE, "",
                      f"{reds}/{N_SEEDS}；rel_step 中位 = {np.median(mags):.5f}")
            H.is_true(reds >= prev, f"Δ/L={dfrac:.2%} 判红率未单调不降")
            prev = reds
        H.exact(rates[0][1], 0, "Δ/L = 0 的基线臂必须全绿")
        H.exact(rates[-1][1], N_SEEDS, "Δ/L = 6% 必须全红")


@H.test(
    "p5c_negative_full_background_subtraction_degenerate",
    intent="**注入被正本点名的退化路径**：把整张背景减掉再测接缝 ⇒ "
           "分母塌到噪声地板，判据在 `Δ = 0` 时也**判红** ⇒ 无判别力",
    inputs="同一夹具；注入实现对每帧减掉**整幅中位背景**（`raw_f − median(raw_f)`）后再测"
           "`rel_step`；`Δ/L ∈ {0, 1.0%, 3.0%}`，各 30 seed",
    expected="注入臂在 `Δ/L = 0` 时也 30/30 判红 ⇒ 「无接缝的产品」被判红；"
             "⇒ 该路径下门失去意义（与保留 `B_ref` 臂的可判定性正面冲突）",
    source="正本 `GATES_AND_TOLERANCES.md` §接缝与 WCS 基线逐字「把整张背景减掉后再比帧间差"
           "是**退化判据**（背景归零时差值天然为零），判据只用保留 `B_ref` 的写法」+ "
           "`PHASE2_UPM.md` §17.2 逐字「产品若把背景扣到近 0（`raw − C_k` 全减路径，§9a 已列为"
           "**退化**路径），`rel_step` **无界**、门失去意义」",
    kind=H.NEGATIVE,
    inject="在测接缝前把整张背景减掉（`raw_f − median(raw_f)`）——一个「先归一化再比」"
           "的常见做法；分母 `bg` 从 ~1000 塌到噪声地板 ~几 ADU",
    defect_id="P5-C-FULL-BACKGROUND-SUBTRACTION",
)
def p5c_negative_full_background_subtraction_degenerate():
    with H.evidence() as ev:
        for dfrac in (0.0, 0.010, 0.030):
            reds, bg_floor = 0, []
            for s in range(N_SEEDS):
                img = step_scene(dfrac * LEVEL, seed=SEED0 + s)
                flat = img - np.median(img)                # ← 注入：全减背景
                reds += int(max_abs_rel_step(flat, BOX) > GATE)
                bg_floor.append(float(np.median(np.abs(flat[0, 0]))))
            ev.record(f"Delta/L={dfrac:.2%} 判红率(full-subtract)", reds / N_SEEDS,
                      GATE, "", f"{reds}/{N_SEEDS}；bg 地板中位 = {np.median(bg_floor):.2f}")
            H.is_true(reds == N_SEEDS,
                      f"全减臂在 Δ/L={dfrac:.2%} 时判红率 {reds}/{N_SEEDS} < 全红 ⇒ 注入未生效")
        ev.record("bg_floor_vs_level", float(np.median(bg_floor)) / LEVEL, 0.0, "",
                  "分母塌到 ~1% ⇒ rel_step 无界（§17.2 逐字）")


# ===========================================================================
# §F P5-d d 扫描判别
# ===========================================================================


@H.test(
    "p5d_d_scan_discriminates_step_from_ramp",
    intent="`d` 扫描把纯台阶与纯梯度分开：纯台阶比值恒 = 1、纯梯度恒 = 4；"
           "混合构型落在两者之间",
    inputs="无噪声夹具；三条臂：①纯台阶 `Δ = 1%·L`；②纯斜坡 `ρ = 0.1%/px`"
           "（指数型，ρ 为局部相对法向斜率）；③两者混合；比值取 "
           "`|rel_step(d=8 px)| / |rel_step(d=2 px)|`",
    expected="① = 1.0、② = 4.0（f64 非归约档内）；③ 落在 (1, 4) 开区间内",
    source="正本条款 + 闭式：`PHASE2_UPM.md` §17.2 逐字「d 扫描判别"
           "（`|rel_step_d4x|/|rel_step|`：**纯台阶 = 1、纯梯度 = 4**）」、§17.3 第 6 条逐字"
           "「纯斜坡的 d 扫描比值恒 = 4、真实台阶恒 = 1，**据此可判别**」；"
           "推导：`4d` 差分对台阶给同一个 Δ、对线性梯度线性放大 4 倍",
)
def p5d_d_scan_discriminates_step_from_ramp():
    want = TB.get("synth.p5.dscan_ratio").value
    arms = {
        "step": step_scene(0.01 * LEVEL),
        "ramp": ramp_scene(0.001),
        "mixed": step_scene(0.01 * LEVEL) + (ramp_scene(0.001) - LEVEL),
    }
    with H.evidence() as ev:
        ratios = {}
        for name, img in arms.items():
            r2 = max_abs_rel_step(img, BOX, d=D_NORM)
            r8 = max_abs_rel_step(img, BOX, d=4 * D_NORM)
            ratio = r8 / r2 if r2 else math.inf
            ratios[name] = ratio
            ev.record(f"{name}: rel_step(d=2)", r2, GATE, "", "")
            ev.record(f"{name}: rel_step(d=8)", r8, GATE, "", "")
            ev.record(f"{name}: d-scan ratio", ratio,
                      want["step"] if name != "mixed" else None,
                      "", "正本 §17.2 逐字；混合臂目标是开区间 (1, 4)")
        H.close(ratios["step"], want["step"], rtol=_base_tol.F64_RTOL, atol=0.0,
                what="纯台阶的 d 扫描比值")
        # 斜坡臂：夹具是**指数**斜坡（ρ 为局部相对斜率），`rel_step = 2·tanh(dρ)`，
        # 故比值闭式是 `tanh(4dρ)/tanh(dρ)`，正本逐字的「= 4」是**小 ρ 线性极限**
        # （偏差 (4x)²/3，x = dρ）。两个数都判：闭式逐位、线性极限在 1e-3 内。
        rho_ramp = 0.001
        x = D_NORM * rho_ramp
        closed_ramp = math.tanh(4.0 * x) / math.tanh(x)
        ev.record("ramp ratio: closed tanh(4x)/tanh(x)", closed_ramp,
                  want["ramp"], _base_tol.F64_RTOL,
                  "指数斜坡闭式；正本的 4 是线性极限")
        H.close(ratios["ramp"], closed_ramp, rtol=_base_tol.F64_RTOL, atol=0.0,
                what="纯斜坡的 d 扫描比值（指数闭式）")
        H.close(ratios["ramp"], want["ramp"], rtol=1e-3, atol=0.0,
                what="纯斜坡的 d 扫描比值（正本线性极限 4）")
        H.is_true(1.0 < ratios["mixed"] < 4.0,
                  f"混合构型的 d 扫描比值 {ratios['mixed']:.4f} 不在 (1,4) 开区间")


# ===========================================================================
# §G P5-e 纯斜坡伪阳（方向相反的第二类边界）
# ===========================================================================


@H.test(
    "p5e_pure_ramp_false_positive_direction",
    intent="真值**无台阶**（Δ = 0）时，光滑法向斜坡单独就能把判据推过门："
           "判据量 = `2dρ`，`ρ ≥ gate/(2d) = 0.25%/px` 即判红",
    inputs="无噪声指数斜坡夹具（ρ = 局部相对法向斜率 /px）；"
           "ρ ∈ {0, 0.0625%, 0.10%, 0.25%, 0.26%, 0.50%} /px",
    expected="① 逐档 `|rel_step − 2dρ| / (2dρ)` ≤ 5.0e-5"
             "（synth.p5.ramp_closed_rel，含 tanh 修正）；"
           "② `ρ = 0.10%/px` 时 `rel_step(d=2) = 0.0040` 判绿但 "
           "`rel_step(d=8) = 0.0160` 判红（越门 1.6×）；"
           "③ `ρ = 0.26%/px` 时 `rel_step = 0.0104` **判红而真值无接缝**",
    source="闭式解析 + 正本 `PHASE2_UPM.md` §17.3 第 6 条逐字「`rel_step = 2d·ρ + Δ/bg`"
           "（ρ = 边界处**相对**法向斜率，无量纲/px），真值**无台阶**（Δ = 0）时该量仍非零 "
           "⇒ ρ ≥ `gate/(2d)` = **0.25%/px** 单独即把判据推过门。合成夹具（ρ 按边界处局部"
           "电平归一）：ρ = 0.26%/px ⇒ rel_step = 0.0104 **判红而真值无接缝**（伪阳）；"
           "ρ = 0.10%/px ⇒ rel_step = 0.0040，而 d 扫描诊断量 `rel_step_d4x`（同式取 "
           "4d = 8 px）已达 0.0160，越门 1.6×（该诊断量在 ρ ≥ 0.0625%/px 时即越门）」",
)
def p5e_pure_ramp_false_positive_direction():
    tol = TB.get("synth.p5.ramp_closed_rel").value
    crit_rho = GATE / (2.0 * D_NORM)
    with H.evidence() as ev:
        ev.record("critical_rho = gate/(2d)", crit_rho, 0.0025, "",
                  "0.25%/px（正本 §17.3 第 6 条逐字）")
        H.close(crit_rho, 0.0025, rtol=1e-15, atol=0.0, what="临界斜率")
        for rho in (0.0, 0.000625, 0.001, 0.0025, 0.0026, 0.005):
            img = ramp_scene(rho)
            r2 = max_abs_rel_step(img, BOX, d=D_NORM)
            r8 = max_abs_rel_step(img, BOX, d=4 * D_NORM)
            closed = 2.0 * D_NORM * rho
            if rho > 0:
                rel = abs(r2 - closed) / closed
                H.less_equal(rel, tol, f"ρ={rho:.4%}/px: rel_step 对闭式 2dρ")
                ev.record(f"rho={rho:.4%}/px rel_step(d=2)", r2, closed, tol,
                          f"相对偏差 = {rel:.2e}（tanh 修正）")
            else:
                H.exact(r2, 0.0, "ρ = 0 的判据量")
            ev.record(f"rho={rho:.4%}/px d-scan diagnostic (d=8)", r8, GATE, "",
                      "诊断量：纯斜坡时恒为 rel_step(d=2) 的 4 倍")
            # 正本逐字的两个锚点
            if abs(rho - 0.001) < 1e-12:
                H.close(r2, 0.0040, rtol=tol, atol=0.0, what="ρ=0.10%/px 的 rel_step")
                H.close(r8, 0.0160, rtol=tol, atol=0.0, what="ρ=0.10%/px 的 rel_step_d4x")
                H.is_true(r8 > GATE, "ρ=0.10%/px 的 d 扫描诊断量未越门（正本称 1.6×）")
                H.is_true(r2 <= GATE, "ρ=0.10%/px 的判决量本身判了红（与正本不符）")
            if abs(rho - 0.0026) < 1e-12:
                H.close(r2, 0.0104, rtol=tol, atol=0.0, what="ρ=0.26%/px 的 rel_step")
                H.is_true(r2 > GATE,
                          "ρ=0.26%/px、真值无台阶，判据未判红 ⇒ 伪阳方向未复现")
            if abs(rho - 0.000625) < 1e-12:
                # 正本逐字「该诊断量在 ρ ≥ 0.0625%/px 时即越门」。ρ = 0.0625%/px 处
                # `rel_step_d4x = 8dρ = 0.01` **恰落在**门上（闭区间端点），故按 `≥` 读；
                # 严格越门从 ρ > 0.0625%/px 起。
                # 正本逐字「该诊断量在 ρ ≥ 0.0625%/px 时即越门」。正本的 1e-2 是
                # **线性极限** `8dρ = 0.01`；指数夹具的 tanh 修正把它压低
                # `(4dρ)²/3 = 8.3e-6` 相对量（落在冻结容差内）⇒ 按「线性极限读数
                # 达到门」判，而不是按「实测恰好 > 门」判。
                linear_limit = 2.0 * (4 * D_NORM) * rho
                ev.record("rho=0.0625%/px d-scan diagnostic", r8, GATE, "",
                          f"实测；正本线性极限读数 = {linear_limit:.6f}"
                          f"（tanh 修正 −{(linear_limit - r8) / linear_limit:.1e}）")
                H.close(linear_limit, GATE, rtol=1e-15, atol=0.0,
                        what="ρ=0.0625%/px 处的线性极限 d 扫描读数")
                H.less_equal((linear_limit - r8) / linear_limit,
                             TB.get("synth.p5.ramp_closed_rel").value,
                             "ρ=0.0625%/px 处的 tanh 修正超出冻结容差")


@H.test(
    "p5e_negative_dhat_candidate_is_not_a_gate",
    intent="候选修法 `Δ̂ = (4·s_d − s_4d)/3` 在纯斜坡上给出 ≈ 0（正是它想要的性质），"
           "**但它的噪声 σ 比判决量本身更大** ⇒ 把同一门值套上去只会抬高低假设虚警；"
           "⇒ 只能作诊断量",
    inputs="①无噪声纯斜坡臂（验证 `Δ̂ ≡ 0`）；②带噪纯斜坡臂"
           "（σ_pix/bg = 1.0703e-2、30 seed，验证 Δ̂ 的散布）",
    expected="① `|Δ̂| ≤ 1.0e-4`（synth.p5.dhat_ramp_abs）；"
             "② 带噪下 `σ_rel(Δ̂) > σ_rel(s_d)` ⇒ 同一 1e-2 门下 Δ̂ 的虚警率**更高**"
             "⇒ 它不能替换判决面",
    source="正本 `PHASE2_UPM.md` §17.3 第 6 条逐字「斜坡不敏感化的候选判据 "
           "`Δ̂ = (4·s_d − s_4d)/3` **仅作为实验证据落盘，尚未进入判决面**：判据面变更须走"
           "变更流程」+ 同条逐字「**`Δ` 项与 `2d·ρ` 项在判据量里加性耦合**，任一项单独越门即"
           "判红」；本条只给该候选量**为什么还不能当门**的定量理由，不改任何阈值",
    kind=H.NEGATIVE,
    inject="把 `Δ̂ = (4·s_d − s_4d)/3` 提为判决量并沿用 1e-2 门——它对纯斜坡不敏感"
           "（这是它被提出的理由），但没人算过它的噪声 σ",
    defect_id="P5-E-DHAT-AS-GATE",
)
def p5e_negative_dhat_candidate_is_not_a_gate():
    tol = TB.get("synth.p5.dhat_ramp_abs").value
    # ① 无噪声纯斜坡
    dhats = []
    for rho in (0.000625, 0.001, 0.0026):
        s2 = max_abs_rel_step(ramp_scene(rho), BOX, d=D_NORM)
        s8 = max_abs_rel_step(ramp_scene(rho), BOX, d=4 * D_NORM)
        dhats.append((4.0 * s2 - s8) / 3.0)
    worst = max(abs(d) for d in dhats)
    # ② 带噪纯斜坡：逐边、逐 seed 的 Δ̂ 散布 vs s_d 散布
    # 逐边、跨 seed 的散布（逐边假警预算要的就是这个 σ；跨边取 max 再算 std
    # 会把边间系统差异混进来，那是 §17.2 的「实测跨边散布」另一件事）
    per_seed = []
    for seed in range(SEED0, SEED0 + N_SEEDS):
        img = ramp_scene(0.001) + np.random.default_rng(seed).normal(
            0.0, SIGMA_PIX_REL * LEVEL, size=(512, 512))
        r2 = seam_readings(img, BOX, d=D_NORM)
        r8 = seam_readings(img, BOX, d=4 * D_NORM)
        per_seed.append({name: (r2[name]["rel_step"], r8[name]["rel_step"])
                         for name in r2})
    ratios_sd, ratios_dh = [], []
    for name in per_seed[0]:
        a = np.array([d[name][0] for d in per_seed])
        b = np.array([d[name][1] for d in per_seed])
        ratios_sd.append(float(np.std(a, ddof=1)))
        ratios_dh.append(float(np.std((4.0 * a - b) / 3.0, ddof=1)))
    s_sd = float(np.mean(ratios_sd))
    s_dh = float(np.mean(ratios_dh))
    with H.evidence() as ev:
        ev.record("noise-free |Dhat| (pure ramp)", worst, tol, "",
                  "纯斜坡下 Δ̂ 的恒等零点")
        H.less_equal(worst, tol, "无噪声纯斜坡上 Δ̂ 不为零")
        ev.record("noisy spread of s_d", s_sd, None, "", "判据量自身的散布")
        ev.record("noisy spread of Dhat", s_dh, None, "",
                  "候选量的散布（理论放大 √17/3 = 1.374 倍）")
        ev.record("spread ratio Dhat/s_d", s_dh / s_sd, 1.0, "",
                  "> 1 ⇒ 同一门下虚警率更高 ⇒ 不能当判决")
        H.is_true(s_dh > s_sd,
                  "Δ̂ 的散布未大于判据量本身 ⇒ 该注入抓不到（需如实登记）")
        # 闭式交叉核对：Var((4a−b)/3) = (16Var(a)+Var(b))/9，a、b 不相关 ⇒ = 17Var/9
        ev.record("closed-form sqrt(17)/3", math.sqrt(17.0) / 3.0, s_dh / s_sd, "",
                  "a、b 独立时的散布放大闭式")


# ===========================================================================
# §H P5-f 加性天光去除的守恒（⚠ 必须测在非控制点）
# ===========================================================================


@H.test(
    "p5f_sky_removal_conservation_off_control",
    intent="`calibrated_f = raw_f − C_f` 的守恒残差必须在**非控制点**处测；"
           "真天光面含二次项（双线性不能精确表示）⇒ 残差非零但有解析上界",
    inputs="512×512 真天光面 `level·(1 + 2e-2·x + 1e-2·y + 3e-2·x² + 1e-2·xy)`；"
           "8×8 control cell 上取该面的采样值作 `θ`；双线性重建 `Ĉ`；"
           "`raw = 真面 + Ĉ` ⇒ `calibrated = 真面`，残差 = 真面 − `Ĉ`；"
           "64×64 查询网格（控制点只占其中 64 个）",
    expected="① 在**非控制点**处 `max|真面 − Ĉ| / level ≤ 1.0e-3`"
             "（synth.p5.sky_bilinear_err_rel，解析上界 c₂/4 = 7.5e-3）；"
             "② `calibrated = raw − Ĉ` 在**全体**像元上等于真面（含控制点）",
    source="正本 `PHASE2_UPM.md` §5 加性校正块逐字 `calibrated_f(p) = raw_f(p) − C_f(p)`、"
           "`C_f(p) = 双线性(8×8 control cell, θ_f)`；解析上界用双线性对二次项的精确误差闭式 "
           "`max|f − f_L| = c₂/4 = |f''|·h²/8`（冻结夹具二次系数 `c₂ = 3.0e-2·level`、归一化边长 `h = 1` ⇒ 7.5e-3）",
)
def p5f_sky_removal_conservation_off_control():
    tol = TB.get("synth.p5.sky_bilinear_err_rel").value
    truth = true_sky_field((512, 512), LEVEL)
    n = 8
    # 8×8 control cell：节点落在 cell 中心（与 weight_chain.h 同一约定）
    centers = (np.arange(n) + 0.5) / n
    jj, ii = np.meshgrid(centers, centers, indexing="ij")
    theta = LEVEL * (1.0 + 2.0e-2 * ii + 1.0e-2 * jj + 3.0e-2 * ii ** 2 + 1.0e-2 * ii * jj)
    # (a) 非控制点测量面：64×64 均匀网格。8 个等分控制节点落在 (j+0.5)/8，
    #     与 linspace(0,1,64) 的采样点**无一重合**（64/8 与节点半格不相容），
    #     所以这个面**全部**是非控制点。
    q = np.linspace(0.0, 1.0, 64)
    QY, QX = np.meshgrid(q, q, indexing="ij")
    chat = bilinear_control_field(theta, QX, QY)
    tq = LEVEL * (1.0 + 2.0e-2 * QX + 1.0e-2 * QY + 3.0e-2 * QX ** 2 + 1.0e-2 * QX * QY)
    # (b) 控制点测量面：精确落在 (j+0.5)/8 的 8×8 网格上
    NY, NX = np.meshgrid(centers, centers, indexing="ij")
    chat_n = bilinear_control_field(theta, NX, NY)
    tq_n = LEVEL * (1.0 + 2.0e-2 * NX + 1.0e-2 * NY + 3.0e-2 * NX ** 2
                    + 1.0e-2 * NX * NY)
    raw = tq + chat
    calib = raw - chat
    with H.evidence() as ev:
        ev.record("off-control pixels", float(QX.size), 4096.0, "",
                  "64×64 均匀网格，与 8×8 控制节点无一重合")
        ev.record("max|true - C_hat|/level (off-control)",
                  float(np.max(np.abs(tq - chat))) / LEVEL, tol, "",
                  "解析上界 c₂/4 = 7.5e-3（冻结门限 1.0e-2）")
        H.less_equal(float(np.max(np.abs(tq - chat))) / LEVEL, tol,
                     "非控制点处的双线性重建误差越出解析上界")
        # 守恒恒等 `calibrated = raw − Ĉ ≡ 真面`：`raw = 真面 + Ĉ` 是浮点加、
        # 还原是浮点减，残差是该往返的舍入（ulp 级）⇒ f64 非归约档，不是逐位 0。
        ev.record("max|calibrated - true|/level (all pixels)",
                  float(np.max(np.abs(calib - tq))) / LEVEL,
                  _base_tol.F64_RTOL, "",
                  "守恒恒等：raw − Ĉ ≡ 真面（浮点往返 ulp 级）")
        H.close(float(np.max(np.abs(calib - tq))), 0.0, rtol=0.0, atol=1.0e-11,
                scale=LEVEL, what="calibrated = raw − C_f 的守恒残差")
        # 登记陷阱面：控制点处残差恒为 0（构造定义，不可作判据）
        ev.record("max|true - C_hat|/level (ON control nodes)",
                  float(np.max(np.abs(tq_n - chat_n))) / LEVEL, 0.0, "",
                  "⚠ 控制点处恒零 = 构造定义，不是判据（见 P5-f 负例）")
        H.exact(float(np.max(np.abs(tq_n - chat_n))), 0.0,
                "控制点处的双线性重建残差")


@H.test(
    "p5f_negative_measure_on_control_points",
    intent="在 8×8 control cell 的**控制点**处测加性校正残差 ⇒ 残差**恒为 0**，"
           "判据恒真、无证据资格",
    inputs="与 P5-f 正例同一真天光面、同一 θ、同一查询网格；只把测量面换成控制点掩膜",
    expected="控制点处残差逐位为 0 ⇒ 无论双线性实现对不对（哪怕把双线性换成常数插值）"
             "该测量都判绿 ⇒ 测量面选错即判据失效",
    source="闭式（双线性在节点处按定义恒等于节点值）+ `PHASE2_UPM.md` §5 逐字 "
           "`C_f(p) = 双线性(8×8 control cell, θ_f)` + `05_INDEPENDENT_TEST_SUITE.md` §1 "
           "「**恒真的比较没有证据资格**」",
    kind=H.NEGATIVE,
    inject="把加性校正残差的测量面从「非控制点」改成「控制点」——最常见的写法"
           "（「就在节点上比一下」），但那里的残差是构造定义、恒为零",
    defect_id="P5-F-MEASURE-ON-NODES",
)
def p5f_negative_measure_on_control_points():
    truth = true_sky_field((512, 512), LEVEL)
    n = 8
    centers = (np.arange(n) + 0.5) / n
    jj, ii = np.meshgrid(centers, centers, indexing="ij")
    theta = LEVEL * (1.0 + 2.0e-2 * ii + 1.0e-2 * jj + 3.0e-2 * ii ** 2 + 1.0e-2 * ii * jj)
    NY, NX = np.meshgrid(centers, centers, indexing="ij")
    chat_n = bilinear_control_field(theta, NX, NY)
    tq_n = LEVEL * (1.0 + 2.0e-2 * NX + 1.0e-2 * NY + 3.0e-2 * NX ** 2
                    + 1.0e-2 * NX * NY)
    on_node = float(np.max(np.abs(tq_n - chat_n)))
    # 注入对照：把双线性换成「最近控制点常数延拓」——一个明显错的实现
    nearest = theta
    wrong_on_node = float(np.max(np.abs(tq_n - nearest)))
    with H.evidence() as ev:
        ev.record("residual ON control nodes (双线性)", on_node, 0.0, "",
                  "恒零 ⇒ 该测量面对实现错误不敏感")
        H.exact(on_node, 0.0, "控制点处的残差")
        ev.record("residual ON control nodes (最近控制点错误实现)",
                  wrong_on_node, 0.0, "",
                  "⚠ 明显错的实现在同一测量面上也给出 0（因为它也按节点取值）")
        H.exact(wrong_on_node, 0.0,
                "错误实现在控制点测量面上的残差（判据抓不到）")


# ===========================================================================
# §I P5-g 两级权重三条性质
# ===========================================================================


@H.test(
    "p5g_two_level_weight_properties",
    intent="`w_UPM`（绝对式）与 `w_cell`（份额式）三条性质："
           "①单元内权比 = w_UPM 之比；②**公共**精度因子（含 k_corr）在份额式内"
           "**严格消去**；③份额式**丢弃跨 control 精度**",
    inputs="一个 control cell 内 3 个观测，quality_factor = [1, 1, 1]、"
           "control_reliability = 1、control_ivar = [1/4, 1/9, 1/16]"
           "（1/3、1/2、1/4 的平方 ⇒ w = SNR²/F_ref² 口径）；"
           "公共精度因子取 k_corr = 1.0 与 1.4 两档（即 control_variance 整体乘 k_corr）",
    expected="① `w_cell[0]/w_cell[1]` 逐位等于 `w_UPM[0]/w_UPM[1]`；"
             "② k_corr 从 1.0 改到 1.4 后 `w_cell` **逐位相同**（公共因子严格消去）；"
             "③ `Σ_cell w_cell = control_reliability` 恒成立 ⇒ 跨 control 精度被丢弃",
    source="正本 `PHASE2_UPM.md` §7「两级权重三条性质」逐字「(i) 同一 control 内两观测的"
           "权比 = `w_UPM` 之比（两式在单元内一致）；(ii) 任何**公共**精度因子（含 `k_corr`）"
           "在份额式内严格消去（**实测 model_hash 位相同**）；(iii) 份额式**丢弃跨 control "
           "精度**（单元总权恒 = `control_reliability`），在 λs>0 时改变估计量 "
           "⇒ 改用绝对权的前提 = λs/λ0 同步换算」",
)
def p5g_two_level_weight_properties():
    ivar_base = np.array([1.0 / 4.0, 1.0 / 9.0, 1.0 / 16.0])
    qf = np.ones(3)
    rel = 1.0
    w1 = w_upm(qf, rel, ivar_base)
    w2 = w_upm(qf, rel, ivar_base / 1.4)          # 公共 k_corr = 1.4
    c1 = w_cell(w1, rel)
    c2 = w_cell(w2, rel)
    with H.evidence() as ev:
        ev.record("w_upm(k_corr=1.0)", w1.tolist(), None, "", "")
        ev.record("w_upm(k_corr=1.4)", w2.tolist(), None, "", "公共精度因子乘 1.4")
        ev.record("w_cell sum (k=1.0)", float(c1.sum()), rel, "",
                  "正本 (iii)：单元总权恒 = control_reliability")
        ev.record("w_cell sum (k=1.4)", float(c2.sum()), rel, "", "")
        # ① 单元内权比
        r_upm = w1[0] / w1[1]
        r_cell = c1[0] / c1[1]
        ev.record("w ratio obs0/obs1 (w_UPM)", r_upm, r_cell, "",
                  "正本 (i)：两式在单元内一致")
        H.close(r_cell, r_upm, rtol=_base_tol.F64_RTOL, atol=0.0,
                what="单元内权比")
        # ② 公共精度因子严格消去。
        # ⚠ 口径说明：正本 §7 (ii) 逐字写的是「**实测 model_hash 位相同**」——那是**序列化
        #   摘要**面（canonical digest 逐位相同），不是中间 double 的逐位相同。
        #   本实现里 `ivar / 1.4` 再求和再相除，FP 舍入使结果相差 ~1 ulp，
        #   所以判据取 `TEST.md` §4「双精度非归约」档 rtol = 1e-12，并把实测 ulp 级偏差落盘。
        #   位精确的那一面（model_hash）由 `upm.cpp` 的序列化承担，不在本层的数值面。
        ev.record("w_cell(k=1.0)", c1.tolist(), None, "", "")
        ev.record("w_cell(k=1.4)", c2.tolist(), None, "",
                  "正本 (ii)：公共精度因子严格消去（正本口径 = model_hash 位相同）")
        rel_gap = float(np.max(np.abs(c2 - c1) / np.maximum(np.abs(c1), 1e-300)))
        ev.record("max relative deviation w_cell(k=1.4) vs w_cell(k=1.0)",
                  rel_gap, _base_tol.F64_RTOL, "",
                  "实测 ulp 级偏差（FP 舍入；正本口径是序列化摘要位相同）")
        H.less_equal(rel_gap, _base_tol.F64_RTOL,
                     "公共精度因子 k_corr 未在份额式内消去")
        # ③ 份额式丢弃跨 control 精度：两个跨 control 精度差 1e6 倍的 cell，
        #    份额式的单元总权完全一样 ⇒ 跨 control 精度不可分辨
        ivar_a = np.array([ivar_base[0], ivar_base[1], ivar_base[2]])
        ivar_b = ivar_a * 1.0e-6                      # 整 cell 精度差 1e6 倍
        s_a = float(w_cell(w_upm(qf, rel, ivar_a), rel).sum())
        s_b = float(w_cell(w_upm(qf, rel, ivar_b), rel).sum())
        ev.record("cell total weight, ivar x1e6 lower", s_b, s_a, "",
                  "正本 (iii)：份额式丢弃跨 control 精度 ⇒ 逐位相同")
        H.exact(s_b, s_a, "份额式的单元总权（跨 control 精度差 1e6 倍）")


# ===========================================================================
# §J P5-h 零尺度分支
# ===========================================================================


def mad_sigma(patch: np.ndarray) -> float:
    """`σ_bg_raw = 1.482602218505602 × median(|x − median(x)|)`（`PHASE2_UPM.md` §5）。"""
    m = float(np.median(patch))
    return MAD_K * float(np.median(np.abs(patch - m)))


def control_statistics(patch: np.ndarray, n_retained: int, k_corr: float):
    """控制点发布量（`PHASE2_UPM.md` §5）。零尺度分支返回 `(ivar=0, var=inf)`。"""
    sigma_bg_raw = mad_sigma(patch)
    same_frac = float(np.max(np.unique(patch, return_counts=True)[1])) / patch.size
    if sigma_bg_raw == 0.0:
        return 0.0, math.inf, sigma_bg_raw, same_frac
    var = k_corr * (math.pi / 2.0) * sigma_bg_raw ** 2 / n_retained
    return 1.0 / var, var, sigma_bg_raw, same_frac


@H.test(
    "p5h_zero_scale_branch_triggers_above_half",
    intent="`σ_bg_raw = 0`（patch 内 ≥ 半数像素同值）时 `control_ivar ≡ 0` 且 "
           "`control_variance` **非有限**；同值占比 ≥ 0.5 触发",
    inputs="17×17 = 289 px patch；同值占比 0.60（174 px 取同一值、其余取互异值）"
           "⇒ σ_bg_raw = 0；`N_retained = 289`、`k_corr = 1.4`",
    expected="`σ_bg_raw == 0`（逐位）、`control_ivar == 0`（逐位）、"
             "`control_variance` 非有限（inf）；同值占比 0.60 ≥ 冻结的 0.5 阈值",
    source="正本 `PHASE2_UPM.md` §5 逐字「`σ_bg_raw = 0`（patch 内 ≥ 半数像素同值）⇒ "
           "无尺度信息：control_ivar 必须为 0，**禁止以数值保护量生成有限方差发布**」+ "
           "`PHASE2_SAMPLER.md` §F5(b) 逐字「σ_bg_raw=0 时断言 `control_ivar == 0` 且 "
           "control_variance 非有限」",
)
def p5h_zero_scale_branch_triggers_above_half():
    thr = TB.get("synth.p5.zero_scale_frac").value
    patch = np.full((17, 17), 7.25)
    n_same = int(math.ceil(0.60 * patch.size))
    flat = patch.ravel().copy()
    flat[:n_same] = 7.25
    # 其余像素取互异值（保证它们不与 7.25 同值，也不彼此同值）
    others = np.arange(1, patch.size - n_same + 1, dtype=float)
    flat[n_same:] = 100.0 + others
    ivar, var, sigma_bg_raw, same_frac = control_statistics(
        flat.reshape(17, 17), 289, 1.4)
    with H.evidence() as ev:
        ev.record("patch size", float(patch.size), 289.0, "", "17×17")
        ev.record("same-value fraction", same_frac, thr, "",
                  "正本 §5 逐字「≥ 半数像素同值」")
        ev.record("sigma_bg_raw", sigma_bg_raw, 0.0, "", "MAD 尺度估计器")
        H.exact(sigma_bg_raw, 0.0, "零尺度 patch 的 σ_bg_raw")
        ev.record("control_ivar", ivar, 0.0, "", "必须逐位为 0")
        H.exact(ivar, 0.0, "零尺度分支的 control_ivar")
        ev.record("control_variance", var, None, "", "必须非有限")
        H.is_true(math.isinf(var), "零尺度分支的 control_variance 是有限值")
        H.is_true(same_frac >= thr, "同值占比未达触发阈值")


@H.test(
    "p5h_negative_below_half_does_not_trigger",
    intent="同值占比 **< 50%** 的 patch **不触发**零尺度分支；"
           "「分支恒触发」的实现必须被判红（否则它抓不到任何有尺度的 patch）",
    inputs="17×17 = 289 px patch；同值占比 0.45（130 px 同值）⇒ σ_bg_raw > 0",
    expected="`σ_bg_raw > 0`、`control_variance` **有限**、`control_ivar > 0`；"
             "⇒ 零尺度分支在此 patch 上不触发（能红能绿）",
    source="正本 `PHASE2_SAMPLER.md` §F5(b) 逐字「断言该分支在 ≥50% 像素同值的 patch 上"
           "触发、在 **<50%** 的 patch 上**不触发**（**正/负例各一**）」+ "
           "`PHASE2_UPM.md` §5 零尺度分支定义",
    kind=H.NEGATIVE,
    inject="把零尺度分支的触发条件从「同值占比 ≥ 半数」放宽成「恒触发」——"
           "于是任何有尺度信息的 patch 都被判成无尺度信息，权重全部归零",
    defect_id="P5-H-ZERO-SCALE-ALWAYS-TRIGGERS",
)
def p5h_negative_below_half_does_not_trigger():
    thr = TB.get("synth.p5.zero_scale_frac").value
    flat = np.empty(17 * 17, dtype=float)
    n_same = 130                       # 130/289 = 0.4498 < 0.5
    flat[:n_same] = 7.25
    flat[n_same:] = 100.0 + np.arange(1, 17 * 17 - n_same + 1, dtype=float)
    ivar, var, sigma_bg_raw, same_frac = control_statistics(
        flat.reshape(17, 17), 289, 1.4)
    with H.evidence() as ev:
        ev.record("same-value fraction", same_frac, thr, "",
                  "正本：< 50% 不触发")
        ev.record("sigma_bg_raw", sigma_bg_raw, 0.0, "", "应 > 0")
        ev.record("control_variance", var, None, "", "应有限")
        ev.record("control_ivar", ivar, 0.0, "", "应 > 0")
        H.is_true(same_frac < thr, "同值占比已达触发阈值（夹具错了）")
        H.is_true(sigma_bg_raw > 0.0, "< 半数同值的 patch 竟给出零尺度")
        H.is_true(math.isfinite(var), "< 半数同值的 patch 给出非有限方差（分支恒触发）")
        H.is_true(ivar > 0.0, "< 半数同值的 patch 给出零逆方差（分支恒触发）")


# ===========================================================================
# §K P5-i 常量场不变量（gauge 对齐）
# ===========================================================================


@H.test(
    "p5i_constant_field_gauge_invariant",
    intent="常数**公共**输入（各帧同值 `raw_f = C`）时 `M = C`、`C_f = 0`"
           "（每分量参考帧 gauge）——**不是** `C_f = C`",
    inputs="3 帧 × 4 个 control，各帧 control 值全为 `C = 1234.5`（binary64 不可精确表示）；"
           "gauge 锚 = 参考帧（frame_id 最小者，第 0 帧），`C_ref = 0`",
    expected="`M = C` 逐位、`C_f = 0` 逐位（全部帧、全部 control）、"
             "`calibrated = raw − C_f = C` 逐位",
    source="正本 `PHASE2_UPM.md` §7 独立不变量逐字「**常量场不变量（SCI-004 gauge 对齐）**："
           "常数**公共**输入（各帧同值 `raw_f=C`）时 `M=C`、`C_f=0`（每分量参考帧 gauge；"
           "弱零锚微调除外），全 control cell 无空间梯度——**不写 `C_f=C`**」+ §5 逐字"
           "「每分量参考帧 = 最小 `frame_id`」",
)
def p5i_constant_field_gauge_invariant():
    c_val = 1234.5
    raw = [np.full((2, 2), c_val) for _ in range(3)]
    m, c_planes = solve_additive_gauge(raw, [np.ones((2, 2)) for _ in range(3)], 0)
    with H.evidence() as ev:
        ev.record("M (public face)", m.tolist(), None, "", "应为 C = 1234.5")
        ev.record("C_f (per-frame planes)", c_planes.tolist(), None, "",
                  "应为全 0（gauge 锚在参考帧）")
        ev.record("max|M - C|", float(np.max(np.abs(m - c_val))), 1.0e-11, "",
                  "f64 非归约档（lstsq 前向误差）")
        ev.record("max|C_f|", float(np.max(np.abs(c_planes))), 0.0, "", "")
        ev.record("max|C_f - C|", float(np.max(np.abs(c_planes - c_val))), 0.0, "",
                  "⚠ 正确读数是 0；写成 C_f = C 会得到 1234.5（P5-i 负例）")
        # 精确算术下 M = C、C_f ≡ 0 逐位成立；本实现走 lstsq，残差是该解的
        # f64 前向误差（12×12 最小二乘），量级 ~1e-12 绝对 ⇒ 取 `TEST.md` §4
        # 「双精度非归约」档：rtol 1e-12 对 C、atol ≥ 1 ulp(C) 对零。
        H.close(float(np.max(np.abs(m - c_val))), 0.0, rtol=0.0, atol=1.0e-11,
                scale=c_val, what="公共面 M = C（f64 非归约档）")
        H.close(float(np.max(np.abs(c_planes))), 0.0, rtol=0.0, atol=1.0e-11,
                scale=c_val, what="逐帧平面 C_f ≡ 0（f64 非归约档）")
        # 校正后的场等于 raw − C_f（构造级：减的是自己减的数）
        calib = raw[2].ravel() - c_planes[2]      # C_f 存成 per-control 向量
        H.close(float(np.max(np.abs(calib - raw[2].ravel()))), 0.0, rtol=0.0,
                atol=1.0e-11, scale=c_val, what="calibrated = raw − C_f")


@H.test(
    "p5i_negative_write_cf_equals_c",
    intent="**注入被正本逐字点名的错误写法**：把 gauge 写成 `C_f = C`（把公共常数"
           "当成逐帧平面扣掉）",
    inputs="与 P5-i 正例同一夹具（3 帧常数 `C = 1234.5`）；"
           "注入实现取 `C_f = C`（对所有帧、所有 control）",
    expected="注入后 `max|C_f − 0| = C = 1234.5 ≫ 0` ⇒ 与常量场不变量正面冲突；"
             "⇒ 该写法必判红",
    source="正本 `PHASE2_UPM.md` §7 逐字「（参考帧之外）……**不写 `C_f=C`**」"
           "—— 正本在同一条里**专门**点出这个错误写法；本条把它做成负例",
    kind=H.NEGATIVE,
    inject="把加性场的 gauge 锚从「参考帧 `C_ref = 0`」改成「所有帧 `C_f = C`」——"
           "表面上公共面还原正确，但逐帧平面被写成了公共常数，"
           "跨帧一致性判据完全失效",
    defect_id="P5-I-CF-EQUALS-C",
)
def p5i_negative_write_cf_equals_c():
    c_val = 1234.5
    raw = [np.full((2, 2), c_val) for _ in range(3)]
    m, c_planes = solve_additive_gauge(raw, [np.ones((2, 2)) for _ in range(3)], 0)
    # 注入：`C_f = C`
    injected = np.full_like(c_planes, c_val)
    with H.evidence() as ev:
        ev.record("correct.max|C_f|", float(np.max(np.abs(c_planes))), 0.0, "",
                  "gauge 锚在参考帧")
        ev.record("injected.max|C_f - 0|", float(np.max(np.abs(injected))), 0.0, "",
                  "正本 §7 逐字「不写 C_f = C」")
        ev.record("injected.max|C_f - C|", float(np.max(np.abs(injected - c_val))),
                  0.0, "", "注入写法的特征量（构造字面量，逐位）")
        H.close(float(np.max(np.abs(c_planes))), 0.0, rtol=0.0, atol=1.0e-11,
                scale=c_val, what="正确实现的 C_f（f64 非归约档）")
        H.is_true(float(np.max(np.abs(injected))) > 1.0,
                  "注入的 C_f = C 未与 gauge 不变量冲突 ⇒ 注入失效")
        H.exact(float(np.max(np.abs(injected - c_val))), 0.0,
                "注入读数 C_f = C")


# ===========================================================================
# §L P5-j Huber 对称性
# ===========================================================================


@H.test(
    "p5j_huber_symmetry_on_dimensionless_z",
    intent="Huber 损失先标准化 `z = r/σ_eff`、`σ_eff = max(|uncertainty|, σ_floor)`，"
           "再对**无量纲 z** 取 `δ = 1.345`：损失关于 z=0 偶对称，"
           "且 `(r, σ_eff)` 同步乘常数时损失不变（尺度无关）",
    inputs="残差 `r` 取 ±1/3、±1、±2、±10（binary64 不可精确表示的 1/3 在内）；"
           "`uncertainty ∈ {0.5, 1.0, 3.0}`、`σ_floor = 0.25`（⇒ `uncertainty` 全部 > "
           "floor，适用域条件 ③ 成立）；δ = 1.345（冻结）",
    expected="① `loss(z) == loss(−z)` 逐位；② `loss(r·k, σ·k) == loss(r, σ)` 逐位"
             "（k ∈ {1/3, 2, 101/17}）；③ 小残差区 `loss = 0.5z²`、大残差区 "
             "`loss = δ|z| − 0.5δ²` 两段与正本逐字式逐位吻合",
    source="正本 `PHASE2_UPM.md` §7 独立不变量逐字「**Huber 对称性（无量纲标准化）**："
           "残差先标准化 `z=r/sigma_eff`，其中 `r=value−M−C`，"
           "`sigma_eff=max(|uncertainty|,sigma_floor)`；`Huber(δ=1.345)` 作用于无量纲 `z`："
           "小残差区 `loss=0.5z²`（等价 L2），大残差区 `loss=δ|z|−0.5δ²`（L1），"
           "位置估计对称」；δ 的出处 = Huber 1964 DOI 10.1214/aoms/1177703732 + "
           "Holland & Welsch 1977 DOI 10.1080/03610927708827533 + Huber & Ronchetti 2009 "
           "ISBN 978-0-470-12990-6（`_tol_chain_b.P5_HUBER_DELTA.source` 逐字承接）",
)
def p5j_huber_symmetry_on_dimensionless_z():
    delta = TB.get("synth.p5.huber_delta").value
    H.exact(delta, 1.345, "Huber 调谐常数")
    r = np.array([1.0 / 3.0, 1.0, 2.0, 10.0, 1.0 / 17.0, 0.5, 4.0, 20.0])
    with H.evidence() as ev:
        ev.record("delta", delta, 1.345, "", "Huber 1964（正本 §7 逐字）")
        for unc in (0.5, 1.0, 3.0):
            sigma_floor = 0.25
            sigma_eff = max(abs(unc), sigma_floor)
            H.is_true(unc > sigma_floor,
                      "适用域条件 ③ 未满足（sigma_floor 主导，95% 效率结论不成立）")
            for rr in r:
                for sign in (1.0, -1.0):
                    z_p = sign * rr / sigma_eff
                    z_n = -sign * rr / sigma_eff
                    H.exact(float(huber_loss(z_p, delta)),
                            float(huber_loss(z_n, delta)),
                            f"Huber 损失关于 z=0 的偶对称（r={rr}, σ_eff={sigma_eff}）")
            # 尺度不变性：r 与 σ_eff 同乘 k ⇒ z 不变 ⇒ loss 不变
            for k in (1.0 / 3.0, 2.0, 101.0 / 17.0):
                z0 = r / sigma_eff
                z1 = (r * k) / (sigma_eff * k)
                # 无量纲性在 FP 上是 ulp 级（`r·k` 与 `σ·k` 各自舍入）⇒ 取 f64 非归约档
                gap = float(np.max(np.abs(huber_loss(z1, delta)
                                          - huber_loss(z0, delta)))
                            / np.max(np.abs(huber_loss(z0, delta))))
                ev.record(f"dimensionless invariance gap (k={k:.4f}, "
                          f"σ_eff={sigma_eff})", gap, _base_tol.F64_RTOL, "",
                          "实测 ulp 级偏差")
                H.less_equal(gap, _base_tol.F64_RTOL,
                             f"Huber 损失不满足无量纲性（k={k}, σ_eff={sigma_eff}）")
            # 两段式与正本逐字式对拍
            z = r / sigma_eff
            a = np.abs(z)
            want = np.where(a <= delta, 0.5 * z ** 2, delta * a - 0.5 * delta ** 2)
            H.exact(float(np.max(np.abs(huber_loss(z, delta) - want))), 0.0,
                    "Huber 两段式（0.5z² / δ|z|−0.5δ²）")
            ev.record("z (σ_eff=%g)" % sigma_eff, z.tolist(), None, "",
                      "无量纲标准化后的残差")
        # ⚠ 适用域登记：sigma_floor 主导时 z 失去统计尺度意义
        unc_small, floor = 0.1, 0.25
        z_floor = r / max(abs(unc_small), floor)
        ev.record("z when sigma_floor dominates", z_floor.tolist(), None, "",
                  "正本 §7 适用域 ③：此时 z = r/σ_floor 退化为固定阈值判据，"
                  "95% 效率结论**不成立**")
        H.exact(float(np.max(np.abs(z_floor - r / floor))), 0.0,
                "sigma_floor 主导时 z 的退化形式")


@H.test(
    "p5j_negative_delta_applied_to_raw_residual",
    intent="**注入把 δ 作用于有量纲的残差 `r` 而不是无量纲 `z`** ⇒ "
           "调谐常数随观测尺度漂移，同一 δ 在不同帧上对应不同的截断点",
    inputs="同一批残差 `r`（含 1/3、1/17 等不可精确表示的值）；"
           "正确实现 `loss = H(r/max(|unc|, floor), 1.345)`；"
           "注入实现 `loss = H(r, 1.345)`（δ 直接吃有量纲的 r）",
    expected="两种口径的损失在 `unc = 1.0` 上**不同**（超出 f64 非归约档）；"
             "且注入口径的截断阈值 `r > δ` 随 `unc` 漂移（正确口径下 `z > δ` 才是尺度无关的）",
    source="正本 `PHASE2_UPM.md` §7 逐字「残差**先标准化** `z=r/sigma_eff`……"
           "`Huber(δ=1.345)` 作用于**无量纲** `z`」+ 同条适用域 ①「z 必须无量纲（已满足）」；"
           "δ 的量纲 = `σ_eff`（`_tol_chain_b.P5_HUBER_DELTA.note` 逐字）",
    kind=H.NEGATIVE,
    inject="把 Huber 的调谐常数从无量纲 z 改到有量纲残差 r 上——"
           "单位换算一改就发生，实现里常被写成「先减模型再判大小」而漏掉除以 σ_eff",
    defect_id="P5-J-DELTA-ON-RAW-RESIDUAL",
)
def p5j_negative_delta_applied_to_raw_residual():
    delta = TB.get("synth.p5.huber_delta").value
    r = np.array([1.0 / 3.0, 1.0, 2.0, 10.0, 1.0 / 17.0, 0.5, 4.0, 20.0])
    floor = 0.25
    gaps = []
    trunc = []
    for unc in (0.5, 1.0, 3.0, 20.0):
        se = max(abs(unc), floor)
        z = r / se
        correct = huber_loss(z, delta)
        injected = huber_loss(r, delta)             # ← 注入：δ 吃 r
        gaps.append(float(np.max(np.abs(correct - injected))))
        # 两种口径下「进入 L1 区」的 r 阈值
        trunc.append((delta * se, delta))
    with H.evidence() as ev:
        ev.record("max|loss_correct - loss_injected|", max(gaps), 0.0, "",
                  "两种口径的损失不同 ⇒ 注入可观测")
        ev.record("L1 截断阈值 r > δ·σ_eff", [t[0] for t in trunc], None, "",
                  "正确口径：随 σ_eff 线性漂移")
        ev.record("L1 截断阈值 r > δ（注入）", [t[1] for t in trunc], None, "",
                  "注入口径：阈值固定 ⇒ 尺度依赖")
        H.is_true(max(gaps) > 1e-6,
                  "把 δ 作用于 r 未改变损失（注入失效，需如实登记）")
        H.is_true(trunc[0][0] != trunc[-1][0],
                  "正确口径的截断阈值未随 σ_eff 漂移")
        H.exact(trunc[0][1], trunc[-1][1], "注入口径的截断阈值")


# ===========================================================================
# §M P5-k k_corr 缩放
# ===========================================================================


@H.test(
    "p5k_kcorr_scaling_linear_and_inverse",
    intent="`control_variance` 随 `k_corr` **线性**缩放、`N_eff = N_retained/k_corr` "
           "**反比**缩放；定义域 `1 < k_corr`（k ≤ 1 显式拒，不 clamp）",
    inputs="σ_bg = 12.0、N_retained = 65、k_corr ∈ {1.0, 1.2, 1.4, 1.8, 2.2}",
    expected="① `Var(k2)/Var(k1)` 对 `k2/k1` 逐条在 f64 非归约档内一致；"
             "② `N_eff(k2)/N_eff(k1)` 对 `k1/k2` 一致；③ `k_corr ≤ 1` 显式拒",
    source="正本 `PHASE2_UPM.md` §7 独立不变量逐字「**k_corr 缩放**："
           "`control_variance` 随 `k_corr` 线性缩放，`N_eff = N_retained / k_corr` "
           "反比缩放；定义域 **1 < k_corr**（§4）」+ §5 逐字 `N_eff = N_retained / k_corr`、"
           "「**1 < k_corr**（k_corr=1 与 k_corr<1 一律显式拒，§4）」",
)
def p5k_kcorr_scaling_linear_and_inverse():
    rtol = TB.get("synth.p5.kcorr_scaling_rtol").value
    sigma_bg, n_ret = 12.0, 65
    with H.evidence() as ev:
        base_k = 1.4
        v_base = base_k * (math.pi / 2.0) * sigma_bg ** 2 / n_ret
        ne_base = n_ret / base_k
        for k in (1.0 + 1e-9, 1.2, 1.4, 1.8, 2.2):
            var = k * (math.pi / 2.0) * sigma_bg ** 2 / n_ret
            ne = n_ret / k
            ev.record(f"k_corr={k}", var, v_base * k / base_k, rtol,
                      "线性缩放")
            H.close(var / v_base, k / base_k, rtol=rtol, atol=0.0,
                    what=f"k_corr={k}: 线性缩放")
            ev.record(f"N_eff(k={k})", ne, ne_base * base_k / k, rtol,
                      "反比缩放")
            H.close(ne / ne_base, base_k / k, rtol=rtol, atol=0.0,
                    what=f"k_corr={k}: N_eff 反比缩放")
        for k_bad in (1.0, 0.999, 0.5, -1.4, 0.0):
            H.raises(ValueError, lambda kb=k_bad: _k_corr_guard(kb),
                     f"k_corr={k_bad} 必须显式拒（不 clamp）")
        ev.record("domain guard", "1 < k_corr", "1 < k_corr", "",
                  "k_corr = 1 与 k_corr < 1 一律显式拒")


def _k_corr_guard(k_corr: float):
    if not (k_corr > 1.0):
        raise ValueError(f"k_corr={k_corr!r} 不在定义域 1 < k_corr（§4 显式拒）")
    return k_corr * (math.pi / 2.0) * 12.0 ** 2 / 65


if __name__ == "__main__":  # pragma: no cover - 人工查阅入口
    for c in H.registered():
        print(f"[{c.kind:8s}] {c.id:52s} {c.intent[:58]}")
    print(f"\n共 {len(H.registered())} 条")
