# RELEASE-02 sky_plane 拟合质量诊断（SKY-FIT）

- 任务：解释 FIX-SKY 后生产天光面的拟合质量指标（`rank=30/174`、`chi2_red=6268`），定位逐帧项 `δ_k` 系统性正偏的根因，给出治本方案与成熟实现对照。
- 对象：**NEW** = `run/RELEASE-02/L4-rebuild/bitref_16w/`（`sky_plane_applied=true`，`weight_mode=1` 等权 mosaic）；输入 `p2_samples.json`（277234 点 / 49 帧）、模型 `p2_sky_plane.bin`。
- 归因人：SKY-FIT。**零 git 写权限；未改任何生产代码/文档；未跑 ninja/cmake/ctest**。
- 日期：2026-09-19。环境：`TMPDIR=/dev/shm/astrocs_skyfit`；证据/脚本落 `run/RELEASE-02/sky-fit/`。
- 复用：`run/RELEASE-02/fix-sky/`（装配/IRLS 复刻）、`run/RELEASE-02/seam/`（per-leaf 边界数据与 `model_H1top.npz`）、`run/RELEASE-02/trail/layout.py`。

---

## 0. TL;DR（结论先行）

| 问题 | 结论 |
|---|---|
| **rank=30/174 是什么** | 这是**诊断口径的分类错误**，不是“174 参数只有 30 个可辨识”。`rank` 只测 **B_ref 约化数据矩阵**（30×30），且代码只能返回 0 或 `n_free=30`（`sky_plane.cpp:987`）。**全 174 维（gauge 固定后）数据系统本身满秩**：rank=174/174（rtol 1e-10），cond=5.05e6；144 个 δ_k 系数全部可辨识（δ 常数 SNR 中位 1129、最小 34）。 |
| **真正的问题** | 不是秩亏/过度参数化，而是**惩罚与数据的量级失配**：`trace(P)/trace(H_red)=1.26e17`，B_ref 被惩罚**完全钉死在惩罚零空间（bilinear）**（自由系数对 bilinear 投影残差 2.8e-16）。即“惩罚承担了 B_ref 形状的全部约束”，但 δ_k 仍由数据约束。 |
| **chi2_red=6268 是否正常** | **不正常**。模型/方差正确时应 ≈1。典型标准化残差 `abs(z)`≈80（中位 82.4），89% 点 `abs(z)>1.345`、64% `>5`。需把方差放大 **6268×** 才 chi2=1。根因：M42（猎户座星云）场天光样本被**真实大尺度星云结构**主导，而模型（共享 bilinear B_ref + 逐帧平面）无法表达。 |
| **δ_k 为何系统性正偏** | **不是 gauge**（`b_k=B_ref+δ_k` 对 {1,ξ,η} 规范不变，`step(B_ref)≈0`，台阶全部来自 `mean_S δ_k`）；**不是稳健权重**（关掉 Huber 更差：ΔM5 −1.24e12→−1.61e12；改均匀权重灾难性变差）；**根因是模型失配 + 样本代表性**：逐帧 δ_k 是**全局平面**，在 M42 星云场里平面无法表达逐帧背景的**空间结构**，平面常数按整帧加权平均给出，在帧子集边界处**过校正 ~1.1e12**（共享 control 上 raw Δ=+2.8e11，模型却给 Δδ_k=+1.39e12）。 |
| **治本方案（排序）** | ① `frame_gradient_order` 1→2（台阶 −3.54e11→−2.01e11，**claim**）；② `roughness_penalty` 按数据尺度归一（→−2.91e11，**claim**，且是正确性修复）；③ 帧子集边界羽化/加权过渡（把台阶变斜坡，**claim**）；④ 重叠区逐帧 DC/电平归一（DrizzlePac `match`/SCAMP 式，**claim**）；⑤ 样本掩膜只留真天光（M42 场几乎无干净天光，风险大）。**单靠模型改动到不了 ≤0.7e11**：order2 最好也只到 ~2.0e11；要 ≤0.7e11 需要羽化或更灵活/局部的逐帧模型并回归验证。 |
| **与成熟实现差距** | DrizzlePac `skymatch/sky` 用**重叠区成对匹配**做天光 equalization（per-image 常数 + sigma clip）；Siril 逐帧**多项式/RBF 背景**（推荐 order 1，明确警告高阶吸收信号）；SWarp 逐帧**网格背景**（BACK_SIZE/BACK_FILTERSIZE）+ 逆方差叠加 + 权重羽化；SCAMP 用重叠源求**相对光度零点**。共同点：**逐帧、局部、独立**估背景 + 重叠匹配 + 边界羽化；我们是**全局共享 B_ref + 逐帧平面联合拟合**且 B_ref 被超强惩罚钉平。 |

---

## 1. 方法与自校验（先证明工具可信）

1. **逐行复刻** `sky_plane.cpp` 的切平面（gnomonic）、均匀 B 样条、δ 基（`[1, η, ξ]`）、Schur 消元、Huber IRLS（`run/RELEASE-02/sky-fit/skyfit_common.py` + `04/07/08`）。
2. **对拍 1（模型）**：Python 复刻的 `λ=1e-3` 解与生产 `p2_sky_plane.bin` 的 B_ref 自由系数一致到 **1.0e-10 相对**、δ_k 一致到 **4e2 绝对（~1e-10 相对）**（`debug_cmp.py`）。
3. **对拍 2（指标）**：用生产模型复算 `rms_weighted=3.365660e12`、`rms_unweighted=7.684795e13`、`chi2_red=6268.2526`，与 artifact `info` **逐位一致**（`02_structure.py`）。
4. **对拍 3（接缝地图）**：用 seam 的 per-leaf 数据复算 H1top 的 mosaic / `mean δ_k` / `mean B_ref`，与 `/dev/shm/astrocs_seam/model_H1top.npz` 的 `prod`/`dmean` **maxdiff=0**；复算的 production 台阶 **−3.5420e11** 与 SEAM 报告 −3.54e11 一致（`05b_compare.py`）。
5. **边界台阶口径**：`J = 右侧−左侧`（K=25 px 窄带逐行线性外推，与 SEAM/VISUAL-B 同法）；`mosaic = mean_elig(raw−C−b)` ⇒ `J_mosaic = J_(raw−C) − J_(mean_S b)`，换模型时 `ΔJ = J_b_prod − J_b_alt`。

> 上述自校验覆盖“模型—指标—接缝地图”三层；后续所有 λ/order/权重实验均在同一复刻上完成，**order1 λ=1e-3 基线复现生产逐位量级**。

---

## 2. 参数分解与 `rank=30/174` 解释

### 2.1 174 个参数的组成（代码事实）

`p2_sky_plane_info.n_params = n_free + (n_frames-1)*m`（`sky_plane.cpp:1038`）：

| 块 | 符号 | 参数数 | 由谁约束 | 证据 |
|---|---|---|---|---|
| 参考天光面 B_ref | 自由样条节点 | **30** | 数据（`H_red` 30×30 满秩，cond 2.17e5）**＋** 粗糙度惩罚 `λDᵀD`（trace 0.228 = **1.26e17×** 数据 trace） | §2.3 |
| B_ref 无支撑节点 | bbox 角点 | 6（`n_full=36`） | 不参与拟合，eval 时最近邻填充 | `sky_plane.cpp:631-641,953-970` |
| 逐帧 δ_k（非参考帧） | 48 帧 × m=3 | **144** | 各帧数据块 `M_k`（3×3，SPD）；**不加惩罚** | `sky_plane.cpp:715-799,918-931` |
| 参考帧 δ_0 | 1 帧 × 3 | **0（固定）** | gauge（`reference_frame`，参考帧=最小 frame_id=cfg7 `M42_M3_T2_20251224`） | `sky_plane.cpp:585,668-707,920` |
| **合计（报告值）** | | **174** | | |

- 网格：`nx=ny=6`、`h=1.0°`、`n_full=36`、`n_free=30`；采样域 u∈[−1.04,1.96]°、v∈[−1.48,1.52]°（约 3°×3°）。
- 原始自由度 `n_full + n_frames·m = 36+147 = 183`，减去 6 个无支撑节点、再减参考帧 3 个 ⇒ **174**。
- 目标场是 **M42（猎户座大星云）**：49 帧 = `M42_M{1..6}_T{2,3}`；这是理解 chi2 与接缝的关键背景。

### 2.2 `rank=30` 的诊断口径（关键纠正）

`sky_plane.cpp:979-992`：

    lam_max = lambda_max_power(H_red, n_free);
    lam_min = lambda_min_inverse(H_red, Ld, n_free);
    rank = (lam_min > cfg.rank_rtol * lam_max) ? (uint64_t)n_free : 0;

- `H_red` 是 **B_ref 的 30×30 约化数据矩阵**（δ_k 已被 Schur profile out），不是 174×174。
- `rank` **只可能取 0 或 30**（布尔满秩标志），它**在结构上不可能**报告“174 里有多少可辨识”。
- 因此 `rank=30/174` 的正确读法是：**“B_ref 约化矩阵满秩 30/30”**，而不是“174 个参数只有 30 个可辨识”。

### 2.3 全 174 维系统本身满秩

用生产数据（逆方差权重）装配 gauge 固定后的 **174×174** 数据正规矩阵 `H`（`02_structure.py`）：

| 量 | 值 |
|---|---|
| 特征值 min..max | `3.622e-24 .. 1.830e-17` |
| cond | **5.05e6** |
| effective rank @ rtol=1e-4 / 1e-5 / 1e-6 / 1e-8 / **1e-10** | 162 / 170 / 170 / **174** / **174** |
| 最小 4 个特征向量能量 | **100% 在 B_ref 块**（bilinear 方向） |
| δ 常数 `abs(δ)/sd`（数据+惩罚协方差） | 中位 **1129**，最小 **34** |

**结论**：
1. **不是模型相对数据过度参数化**：277234 观测 vs 174 参数，全系统满秩，δ_k 全部可辨识。
2. **不是（全系统）惩罚承担全部约束**：δ_k 无惩罚、由数据约束。
3. **但 B_ref 的形状确实由惩罚承担**：见 §2.4。

### 2.4 惩罚量级：B_ref 形状 100% 由惩罚决定

| 量 | 值 |
|---|---|
| `trace(H_red)` | 1.804e-18 |
| `trace(P)=λ·tr(DᵀD)`，λ=1e-3 | 0.228 |
| `trace(P)/trace(H_red)` | **1.264e17** |
| B_ref 自由系数对惩罚零空间 {1,ix,iy,ix·iy} 的投影残差 | **2.75e-16**（即 B_ref ≡ bilinear） |

即：`λ=1e-3` 是**为 O(1) 权重标定的**绝对惩罚，而生产权重是逆方差 `~1e-21`；相对数据而言惩罚被放大 `~1e17`，B_ref 的 cubic 自由度被完全压掉，退化为 bilinear（平面+扭转）。**这不是秩亏，而是尺度/语义问题**（FIX-SKY 已标记，属科学/语义选择）。

---

## 3. `chi2_red=6268` 解释

### 3.1 复算（与 artifact 逐位一致）

| 指标 | 值 | artifact |
|---|---|---|
| `rms_weighted` | 3.365660e12 | 3.365660e12 |
| `rms_unweighted` | 7.684795e13 | 7.684795e13 |
| `chi2_red = Σw r²/(n_used−n_params)` | **6268.2526** | 6268.2526 |

### 3.2 量纲/尺度

- 权重 `w=1/control_variance`：mean w=5.53e-22，mean σ=8.28e10，median σ=4.23e10。
- 残差：median `abs(r)`=3.32e11，`std(z)=76.9`，median `abs(z)`=82.4，`frac abs(z)>1.345=0.892`，`>5=0.644`。
- **`chi2_red=6268 ⇒ mean(w r²)=6268 ⇒ 典型 abs(z)≈79`**。要让 chi2=1 需把每点方差放大 **6268×**。

### 3.3 是否“正常”？—— 不正常，且不是“高/低”问题，是模型失配

- 若模型正确且方差正确，`chi2_red≈1`。6268 比 1 大 **3.8 个数量级**，等价于残差 ~79σ。
- 但 `rms_unweighted=7.68e13` ≈ **7.7× 天光电平（~1e13）**：残差不是噪声量级，而是**真实星云结构**。M42 场里“background-clean control patch”仍包含星云发射；patch estimator 的 `variance` 只是 patch 中位数的**统计**误差（~4e10），不是天空结构变化的度量。
- **所以**：`chi2_red` 大是“模型（共享 bilinear B_ref + 逐帧平面）无法表达真实大尺度背景 + 方差只含统计误差”的必然结果；它**不能**被解释为“拟合很紧”或“权重过松”。
- 附带影响：`n_rejected=178536`（64% 点 `abs(z)>5`）——最终统计用 base 权重（不含 Huber），但 IRLS 中 Huber 把这些点降权，**有效拟合由少数低残差点主导**。这是 chi2 巨大与 δ_k 偏差的背景之一，但不是接缝的独立根因（§4.3）。

> FIX-SKY 标记的“λ 物理量级是否需按数据尺度归一”在本轮得到确认：**需要**（§5.2），且归一后 B_ref 才重新由数据决定形状。

---

## 4. δ_k 系统性正偏的根因

### 4.1 现象与量化

- δ_k 常数 std=2.15e12，范围 −4.23e12（cfg35 `M4_T3_20251212`）… +7.41e12（cfg37 `M4_T3_20251228`）。**大正 δ_k 并非 M5 独有**（cfg23 +5.85e12、cfg37 +7.41e12、cfg45 +4.99e12）。
- SEAM 关注的 H1top 边界进入帧是 **8 个 M42_M5 帧**（cfg 11/12/13/38–42），其 δ_k 常数 +3.58…+7.33e12。
- 我独立复算 H1top 台阶（`05b_compare.py`）：`J_mosaic=−3.542e11`，`J(mean_S δ_k)=+4.216e11`，`J(mean_S B_ref)≈0` ⇒ **台阶 100% 来自逐帧项，与 SEAM 一致**。

### 4.2 排除 gauge（**不是 gauge 问题**）

- 模型对 mosaic 的贡献是 `b_k=B_ref+δ_k`。`gauge_mode=1 (sum_zero)` 只是把 δ 常数整体平移、`gauge_shift` 补偿，`b_k` **逐点不变**。
- 实测 `step(B_ref)≈0`（+3.8e-2），台阶完全来自 `mean_S δ_k`。
- ⇒ 改 gauge **不会改变接缝**。

### 4.3 排除稳健权重（**不是稳健权重问题**）

（`07_weights.py`，同一复刻）

| 权重方案 | δ 常数 std | 跨帧 corr std 中位 | p84 | ΔM5 中位 |
|---|---|---|---|---|
| 生产：逆方差 + Huber | 2.15e12 | 8.35e10 | 3.78e11 | −1.24e12 |
| 逆方差，**无 Huber** | 2.85e12 | 9.64e10 | 4.71e11 | −1.61e12 |
| 均匀权重，无 Huber | 7.10e13 | 2.92e11 | 3.31e12 | −6.8e11 |
| 均匀权重 + Huber | 5.90e14（发散） | 1.97e12 | 6.54e13 | −1.18e14 |

- **关掉 Huber 反而更差**；均匀权重灾难性变差 ⇒ **逆方差 + Huber 是必要的，不是病根**。
- 加权 vs 未加权的 M5−非M5 corr 差：−1.19e12 vs −1.24e12（几乎相同）⇒ 权重错配也不是主因。

### 4.4 样本掩膜/代表性（**部分是，但不是掩膜 bug**）

- 所有观测 `quality_flags=0`，`n_masked=0`；sampler 的高污染拒绝只 1841/358976。**没有掩膜泄漏证据**。
- 但**样本对 mosaic 不具代表性**：在 2315 个 M5 与非M5 共同观测的 control 上（`09_weighted_dm5.py`）：
  - 原始电平差 `Δval(M5−非M5)` 中位 **+2.75e11**；
  - 模型却给出 `Δδ_k` 中位 **+1.39e12**；
  - 校正后 `Δcorr` 中位 **−1.24e12** ⇒ **过校正 ~1.1e12**。
- H1top 竖直剖面（`06_boundary_profile.py`）在边界下侧（y≈1185–1240）：`ΔrawC≈+0.3…+0.6e12`，而 `Δcorr≈−1.3…−1.6e12` ⇒ 模型在 mosaic 叶级过校正 ~1.7e12。
- **结论**：control 样本（“background-clean”）在星云场里**不等于天光**；逐帧平面按整帧样本的加权平均定常数，边界处的真实电平与之不符。

### 4.5 根因（机制）

1. **模型失配（主因）**：真实背景在 M42 场有 1e12–1e13 量级的二维结构；模型每帧只有 `B_ref(bilinear) + δ_k(平面)`，**无法表达**。
2. **惩罚放大**：B_ref 被 1.26e17 倍惩罚**钉死在 bilinear**，无法承担“所有帧共享的大尺度结构”这一唯一可共享分量；于是**结构化背景全部压给逐帧 δ_k**。
3. **逐帧平面的常数是整帧折中**：进入子集（M5）在边界处的实际电平只比在位帧高 ~0.3e12，但平面常数被整帧（含更亮区域）拉高 ~1.4e12 ⇒ 扣除后在边界形成 **−1.2e12 的局部台阶**。
4. **接缝 = 帧子集平均的不连续**：`mean_S δ_k` 在 n 变化处跳变（+4.22e11）；原始帧台阶仅 +0.93e11 ⇒ **模型把台阶放大 4.5×**。

一句话：**δ_k 正偏不是数值/规范/权重缺陷，而是“用全局平面拟合结构化天光”的模型误差在帧子集边界上的表现。**

---

## 5. 治本方案（按性价比排序）

> 所有方案均**不破坏 B_ref 全局光滑性**（B_ref 是共享面，其空间变化不会造成子集台阶；羽化/局部模型也不改 B_ref）。
> “claim”=涉及冻结科学默认/产品语义，须走 `ENGINEERING_SPEC.md §3` 变更 claim + 一致性回归。**本轮未改任何代码。**

台阶基线（H1top，生产）：**−3.542e11**（目标 ≲0.7e11）。下表为 Python 复刻预测（`05c/08d`）。

| # | 方案 | 预期效果（H1top 台阶） | 逐帧散差 | 风险 | 是否需 claim |
|---|---|---|---|---|---|
| **1** | **`frame_gradient_order` 1→2**（逐帧二次，m:3→6） | **−3.54e11 → −2.01e11（−43%）** | corr p84 3.78e11→2.77e11；ΔM5 −1.24e12→−0.52e12；rms_w 3.37e12→3.16e12 | 参数 174→318；逐帧更灵活可能拟合帧内结构/噪声；**order2 后 bilinear(ξη) 可被 δ_k 吸收 ⇒ B_ref 的 bilinear 部分变成 gauge，语义变化**；须回归（含 star flux 不破坏） | **是**（SCI/ALG 默认变更） |
| **2** | **`roughness_penalty` 按数据尺度归一**（λ_eff ≈ λ0·w_scale，或权重归一后 λ0=1e-3） | −3.54e11 → **−2.91e11（−18%）** | corr p84 3.78e11→3.00e11 | 改变冻结默认 λ；λ→0 时 B_ref 可在节点尺度过拟合（但仍是 C² 光滑面，**不产生台阶**）；须定归一规则 | **是**（科学/语义选择；FIX-SKY §7 已标记） |
| **3** | **帧子集边界羽化/加权过渡**（进入/退出帧几像素权重渐变） | 把残余台阶转成平滑斜坡；**唯一能把可见台阶压到 ≲0.7e11 的工程手段** | 不变 | 改变边界有效 PSF/噪声、可偏测光；属产品语义变更；不修科学根因 | **是**（产品语义） |
| **4** | **重叠区逐帧 DC/电平归一**（DrizzlePac `skymatch/match`、SCAMP 相对光度式） | 直接消除帧级分量（接缝主项）；预期台阶降到逐帧结构残差量级（需实测） | 可能显著改善 | 新增算法/接线；与 UPM C 场/MA 定标的先后关系须定义；须回归 | **是** |
| **5** | **样本掩膜只留真天光**（排除星云结构） | 阻止模型拟合星云；但 M42 场几乎无干净天光 ⇒ 大片区域模型无约束 | 不确定 | 覆盖空洞、fail-closed、产品语义；可能把接缝换成空洞 | **是** |
| **6** | **`node_spacing_deg` 减小**（1.0→0.5°，配 λ 归一） | 让共享 B_ref 表达**公共**结构；但接缝是**逐帧**差异 ⇒ 预期小 | 小 | 节点数↑；仍受惩罚；单独无效 | 是（默认变更） |
| — | **不建议**：改 `weight_mode` 修接缝 | 逆方差/Huber 不是病根，均匀权重更差 | — | — | — |
| — | **不建议**：盲目 order≥3 | order3 把台阶**反号**（+2.32e11），ΔM5 近零但边界不单调改善 ⇒ 需专门验证 | p84 3.00e11 | 过拟合帧结构 | 是（且 C++ 现钳位 order≤2，`sky_plane.cpp:431`） |

**关键诚实结论**：把逐帧散差降到 ~2e11 **并不**自动使台阶 ≤0.7e11——order2 把 corr p84 降到 2.77e11，但边界台阶仍有 2.0e11。台阶由“进入帧在自身足迹边缘的校正误差”主导，需**羽化**或**更灵活/局部的逐帧模型**并逐案回归，不能只靠全局统计量外推。

---

## 6. 与成熟实现的对照

| 实现 | 帧间背景匹配做法 | 与我们的差距 |
|---|---|---|
| **DrizzlePac**（`sky`/`skymatch`） | `skymethod ∈ {localmin, globalmin, match, globalmin+match}`；`match` **在重叠区成对比较天光**做 equalization；`skystat ∈ {median,mode,mean}` + `skyclip/skylsigma/skyusigma` 裁剪；天光是**逐图像常数**（非平面）；另有 `photeq` 做逆灵敏度（光度）equalization。 | 我们用**联合拟合的逐帧平面**而非逐图常数，且与全局 B_ref 耦合；缺少**显式重叠区成对匹配**这一稳健环节。 |
| **Siril** | 背景提取：sample-based 多项式（`-order` 1–3）或 sample-free 多尺度/RBF（`-smooth`）；文档明确**推荐 order 1**，警告高阶会把信号（星云）吸收进背景；`linear_match` 用公共星做帧间线性匹配。 | 我们固定 order 1 逐帧平面（方向一致），但**共享 B_ref 被超强惩罚钉平**、且样本在星云场不干净——Siril 明确提醒的问题正是我们的现场。 |
| **SWarp**（+SCAMP） | 逐帧**背景网格** `BACK_SIZE` + 中值滤波 `BACK_FILTERSIZE`，`BACKPHOTO_TYPE`；叠加为逐像素**逆方差** `out=Σ(x_k/var_k)/Σ(1/var_k)`、`RESCALE_WEIGHTS`；帧间**相对光度零点由 SCAMP** 用重叠源求解（Bertin 2006；SDSS 重叠相对定标 Padmanabhan+2008）。 | SWarp 是**逐帧独立局部背景 + 重叠相对光度**；我们是**全局共享面 + 逐帧平面联合**。SWarp 的局部网格在帧子集边界处天然连续，我们的平面会外推。 |
| **photutils / SExtractor** | `Background2D`（`box_size` 网格 + `SigmaClip`）逐图估背景；SExtractor 分块背景网格。 | 同为逐图局部网格；我们无网格背景，只有全局样条+逐帧平面。 |
| **SCAMP** | 只解**相对光度零点与天体测量**，无像素背景归一（本项目 `docs/research/SNR_WEIGHT_RESEARCH_PACK.md` §53 已核验）。 | 我们的 UPM 天光面**不引 SCAMP 为依据**（同前）；但帧间电平问题可借鉴其重叠相对定标。 |

**共性差距**：成熟实现普遍采用 **(a) 逐帧、局部、独立**的背景估计（网格/多项式/RBF）、**(b) 显式重叠区成对匹配/相对光度零点**、**(c) 帧边界权重羽化**。我们目前是 (a) 全局共享 B_ref + 逐帧平面联合、无 (b)、无 (c)；在 M42 这类强结构场，(a) 的刚性 + 无 (b)(c) 共同放大了接缝。

**文献/实现依据**
- DrizzlePac `sky`/`skymatch`：https://drizzlepac.readthedocs.io/en/deployment/sky.html ；`photeq`：https://drizzlepac.readthedocs.io/en/deployment/photeq.html
- SWarp 手册（`BACK_SIZE`/`BACK_FILTERSIZE`）：https://star.herts.ac.uk/~pwl/Lucas/rho_oph/swarp.pdf ；项目：https://www.astromatic.net/software/swarp/
- Siril 背景提取：https://siril.readthedocs.io/en/latest/processing/background.html ；命令：https://siril.readthedocs.io/en/latest/Commands.html
- photutils `Background2D`：https://photutils.readthedocs.io/en/latest/user_guide/background.html
- SCAMP：Bertin, E. 2006, ASP Conf. Ser. 351, 112；Padmanabhan, N. et al. 2008, ApJ 674, 1217（重叠相对定标/gauge）
- 项目内权威：`docs/plugins/algorithms_phase2/11_upm.md` §4.3–4.5（模型/gauge/失败条件/默认值）；`docs/research/SNR_WEIGHT_RESEARCH_PACK.md` §47–53（SWarp/Siril/SCAMP 对照）

---

## 7. 诚实边界

- **未改任何生产代码/文档；未跑 ninja/cmake/ctest；零 git 写操作。**
- 所有 λ/order/权重实验是 **Python 逐行复刻**（`sky_plane.cpp` IRLS + FIX-SKY 的锚+deflation 求解）；基线 order1 λ=1e-3 与生产 artifact 一致到 1e-10（B）、指标逐位一致、接缝地图 maxdiff=0，故实验可信；但 C++ 未实跑，末位可能有差。
- `frame_gradient_order=2` 生产可达（默认钳位 ≤2）；**order3 当前不可达**（`sky_plane.cpp:431` 钳位），仅作趋势证据。
- 台阶绝对幅度有 ±20–30% 方法学不确定度（基线 K、边界轨迹、与 VISUAL-B 口径差）；**符号、量级、排序**在多法下一致。
- 我**未能**独立验证“逐帧平面过校正”的绝对真值（无独立绝对定标）；只能说它是台阶的主导来源，且与“control 样本不代表 mosaic 叶级”一致。
- `rank` 字段的口径问题（只报 B_ref 约化秩）与 `chi2_red` 缺量纲注释，属**诊断可读性缺陷**；建议单独立项（非本轮改动）。
- H2（带内偏移）未在本轮重做；沿用 SEAM 结论。

---

## 8. 证据/脚本索引（`run/RELEASE-02/sky-fit/`）

| 文件 | 内容 |
|---|---|
| `skyfit_common.py` | 切平面/B 样条/δ 基/加载（复刻 `sky_plane.cpp`） |
| `00_cache.py` | 缓存设计矩阵与样本（`/dev/shm/astrocs_skyfit/`） |
| `01_frames.py` | 帧清单与模型 δ_k 常数 |
| `02_structure.py` | **174 维系统谱/rank/cond**、chi2、B_ref 惩罚零空间投影、参数协方差 |
| `03_perframe.py` | 逐帧 δ_k、跨帧重叠一致性、ΔM5、逐对帧偏移 |
| `04_lambda.py` | λ 尺度实验（1e-3 / 1e-21 / 0） |
| `05_seam_alt.py`、`05b_compare.py`、`05c_seam_alt.py` | 与 seam 地图对拍 + 换 λ 的 H1top 台阶预测 |
| `06_boundary_profile.py` | H1top 竖直剖面（M5 vs 非M5 的 raw/corrected 电平） |
| `07_weights.py` | Huber/均匀/逆方差 权重对照 |
| `08_order2.py`、`08d_order_step.py` | order 1/2/3 对照 + 台阶 |
| `09_weighted_dm5.py` | 加权 vs 未加权的 ΔM5 |
| `debug_*.py` | 模型/指标对拍与残差口径校验 |
| `*.json` | 各步结构化结果 |
| `/dev/shm/astrocs_seam/*` | 复用 SEAM 的 per-leaf 边界数据与 `model_H1top.npz` |
