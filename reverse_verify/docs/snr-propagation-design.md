# 信噪比（SNR）在全流程中的传播：从单帧校准到成品的方案设计

> **论文雏形** — 本文是 AstroCS RELEASE-02 `SNR-DESIGN` 工作项的设计文档，可直接扩写为论文的方法节。
> 工作区：`reverse_verify/`（逆向验收区）。所有数值证据由 `reverse_verify/experiments/snr_design/exp1..exp5` 独立产出，
> 中间产物落 `run/reverse_verify/snr_design/`。
> **性质**：只做方案设计，不改任何生产代码（`lib/`、`docs/`、`tests/`、`ci/` 零改动），零 git 写。

---

## 0 摘要与贡献

本文回答四个问题：（i）校准后单帧标准 FITS 的 SNR 应当如何定义与计算，天光如何被正确处置；
（ii）为什么"一帧一个总 PSF SNR"不够，稀疏控制点 SNR 层应如何布局、由 Phase2 重建为稠密 SNR，
以及这一稀疏化的精度损失是多少；（iii）SNR 如何经 UPM/天光面拟合与逆方差叠加传播到标准面模型；
（iv）成品 SNR 的公式、产品表达与误差预算。

**主要结论（可独立复核）**：

1. **单帧 SNR 必须是"通量型、信号已扣独立背景、天光只进噪声"的口径**：
   `SNR_k(F_ref) = F_ref / σ_F,k`，`σ_F,k⁻² = P_kᵀ C_k⁻¹ P_k`。天光**从信号中扣除**、**作为泊松噪声项保留在 σ 中**，
   二者不可混淆（§2.2）。
2. **权重误差的代价是二阶的、且只由帧间散射决定**：
   `Var_approx/Var_opt = 1 + Var_p(δ) + O(δ³)`，`δ_f = ŵ_f/w_f − 1`（EXP-1，MC 与解析在 5 位小数一致）。
   **共模权重误差完全免费**（实测 ratio = 1.000000）——这是稀疏 SNR 层可被接受的定量依据。
3. **归一化后的方差必须写成"残差制造者"形式**：
   `Var(corrected_i) = Σ_j P_ij² σ_j²`，`P = I − H`；朴素写法 `σ² + Var(ĝ)` 在 `N=8` 时高估 `9/7 = 1.2857×`（EXP-1）。
4. **稀疏→稠密重建的精度损失可解析给出**：kriging 误差由 `Δ/ℓ` 单参数控制（EXP-4）；
   `Δ ≲ ℓ` 才有效；双线性在 `Δ ≥ 2ℓ` 时**比常数还差**（EXP-4）。实测真实帧 `ℓ ≈ 48 px`（EXP-2），
   Phase-2 UPM 控制点间距 64 px 与之一致，而 Phase-1 的 8×8 patch 网格（512 px）**过疏**。
5. **稀疏 SNR 层只在"帧间相对权重随位置变化"时才有增益**。实测：同一 tile 的 4 帧 σ 场几乎相同（帧间散差 1.058），
   此时帧级标量权重的效率损失仅 **0.063%**，而 64 px 稀疏重建反而带来 **6.8%** 的效率损失（EXP-3）。
   这是"判据先行"的直接后果：**先证明空间权重有增益，再启用稀疏层**。
6. **当前生产链的成品 σ 相对误差约 131%**，主导项不是统计噪声而是**帧标量 σ 的源污染偏差**
   （实测 `σ_MAD/σ_clippedRMS = 1.3127`，EXP-3 Part A）；修好它 + 传播 `C_θ` 后降到 ~3.7%（良态）或 ~220%（病态）（EXP-5）。

---

## 1 记号与全流程数据流

### 1.1 记号

| 符号 | 含义 | 单位 |
|---|---|---|
| `r_p, b_p, d_p, f_p` | 原始帧、母版 bias、母版 dark（已减 bias）、归一母版 flat 的第 p 像素 | ADU, ADU, ADU, 1 |
| `α = t_light/t_dark` | 暗电流曝光比 | 1 |
| `y_p` | **校准后**（calibrated/cleaned）标准 FITS 像素值 | ADU |
| `g_phot` | Phase-1 光度响应（整帧标量乘法） | 1 |
| `I_p` | 施加光度响应后的帧像素 = `g_phot · y_p` | ADU |
| `B_p` | 局部天光背景（真值） | ADU |
| `S_p` | 源的期望计数（真信号） | ADU |
| `σ_pix,p` | 逐像素随机噪声标准差（空背景） | ADU |
| `P_p` | 归一化点源轮廓 `Σ_p P_p = 1` | 1 |
| `A_NEA = 1/Σ_p P_p²` | 噪声等效面积 | px² |
| `a_k` | 帧 k 的光度响应（把本帧映射到公共通量尺度） | 1 |
| `F_ref` | **组内公共**参考通量 | ADU |
| `σ_F,k` | 点源通量的不确定度 | ADU |
| `W_info,k = a_k² P_kᵀ C_k⁻¹ P_k = 1/σ_F,k²` | 点源信息权重 | ADU⁻² |
| `w_k(p)` | 叠加权重 | ADU⁻² |
| `C_f(p), δ_k(p), g_k` | UPM 加性校正场、逐帧加性偏差、乘性响应 | ADU, ADU, 1 |
| `θ` | UPM 参数向量 | — |
| `C_θ = (JᵀWJ)⁻¹` | 参数协方差 | — |
| `H, P = I − H` | 残差制造算子 | — |

### 1.2 数据流（SNR 视角）

```text
raw  ──校准(§2.1 噪声方程)──▶  calibrated/cleaned 标准 FITS
                                     │
                     ┌───────────────┴────────────────┐
                     ▼                                ▼
         逐像素方差 σ²_pix(x,y)              点源信息权重 W_info(x,y)
         （掩膜 patch MAD）                  （PSF 轮廓 + σ_pix + a_k）
                     │                                │
                     └───────────┬────────────────────┘
                                 ▼
                    ┌────────────────────────────┐
                    │  ① 帧级总 SNR：SNR_k(F_ref) │  写入 HiPS 头（唯一帧级参考）
                    │  ② 稀疏帧内 SNR 层（可选）   │  控制点 {x,y,SNR_ref}
                    └────────────────────────────┘
                                 ▼  Phase2
              ┌──────────────────────────────────────────┐
              │ 稠密 SNR 重建（kriging / 样条 / 低阶基）    │
              │ UPM 加权拟合 → C_θ → C_out = C_stat + J_out C_θ J_outᵀ │
              │ 叠加 w_k(p) = 1/Var(corrected_k(p))        │
              └──────────────────────────────────────────┘
                                 ▼
              ivar_mosaic = Σ w_k ,  variance = 1/Σ w ,  SNR_mosaic = √(Σ w)·F̂
```

---

## 2 ① Phase 1：单帧 SNR 如何计算

### 2.1 从校准后标准 FITS 出发的逐像素方差模型

#### 2.1.1 校准方程与方差传播（目标态）

生产校准式（`docs/science/CALIBRATION.md:65-75`，与 IRAF `ccdproc`/LSST `ip_isr` 一致）：

```text
dark_opt=0:  y_p = ( r_p − b_p − α·d_p ) / max(f_p, 0.1)
dark_opt=1:  y_p = ( r_p − b_p − α·(d_p − b_p) ) / max(f_p, 0.1)
α = t_light / t_dark
```

逐像素方差的**完整一阶传播**（`docs/plugins/algorithms_phase1/01_calibration.md:31`，标记为"目标态"）：

```text
V(y_p) = { V(r_p) + V(b_p) + α²[ V(d_p) + V(b_p) ] + y_p² V(f_p) } / f_p²      (2.1)
```

逐项物理来源：

```text
V(r_p) = S_p + B_p + I_dark,p + σ_read²        [e⁻]      （源 + 天光 + 暗流 + 读出，泊松/高斯）
V(b_p) = σ_bias² + V_master,bias               [ADU²]    （读出 + 母版有限帧数）
V(d_p) = σ_dark² + V_master,dark               [ADU²]
V(f_p) = σ_flat² + V_master,flat               [ADU²]    （平场光子噪声 + 母版散度）
```

写成 ADU 单位（gain `G` [e⁻/ADU]、读出噪声 `RN` [e⁻]、暗流 `D` [e⁻]）：

```text
V(r_p) [ADU²] = ( S_p + B_p + D_p + RN² ) / G²
```

**这是"教科书完整式"，但生产上不能直接用**：AstroCS 的校准后帧头**没有 GAIN / RDNOISE / SATURATE / DATAMAX**
（实测：`run/RELEASE-02/L4-rebuild/norm/t2_m1_red/calibrated_*.fts` 头 117 张卡，无上述任一键）。
因此本项目冻结的**生产基线是经验式**，而不是 CCD 方程：

```text
σ_bg(x,y) = 1.482602218505602 · median( |x − median(x)| )      # MAD→σ，Gaussian 假设
variance(x,y) = max( σ_bg²(x,y), 1e-12 )
ivar(x,y)     = 1 / variance(x,y)
```
（`docs/science/NOISE_MODEL.md:51-63`；常数 `1.482602218505602 = 1/Φ⁻¹(3/4)` 为冻结值。）

**设计决策 D1（噪声方程口径）**：采用**分层混合式**——

```text
σ²_pix(x,y) = max( σ²_emp(x,y) , σ²_model,pixel(x,y) )                         (2.2)
σ²_emp(x,y)  = 掩膜 patch 的 MAD 方差（经验、无偏于源污染）
σ²_model,p(x) = S_p/G + B_p/G + D_p/G + RN²/G²   （当 gain/RN/暗流元数据可用时）
```

- **理由**：经验项对未建模的系统效应（平场残差、坏点、微弱源污染）稳健；模型项提供**物理可分解性**
  （可回答"这一帧的噪声里天光占多少"）与**元数据可用时的外推能力**。取 `max` 保证不低估。
- **依据**：`docs/science/NOISE_MODEL.md:134,139` 明确"生产唯一基线为 empirical MAD（`source==0`）"，
  gain 模型"仅诊断路径（SNR-005 交叉验证），不入生产"（`NOISE_MODEL.md:66-69`）——本文的 D1 是**在保留该冻结结论的前提下**
  增加一个受门控的物理项，属需走 SCI 变更 claim 的改动，本文只给设计。
- **不借鉴**：不采用 SExtractor 的 `mode/median` 迭代 σ（网格尺寸、尺度估计器、场基三者都不同，
  `NOISE_MODEL.md:183` 已明确二者**不等价**）；不采用 PixInsight 的 `N*_MAD = 2.48308·MAD` 与 `N*_Sn = 2.03636·S_n`
  （定义域不同，`NOISE_MODEL.md:202` 明令不得互换）。

#### 2.1.2 掩膜与天空预算（抑制源污染）

经验项的偏差来源是**未分辨源与星翼混入"空背景"样本**。冻结的处置（`NOISE_MODEL.md:75-91`）：

```text
Gaussian : r_local = σ_p·sqrt( 2·ln( F / (2π σ_p² k σ_bg) ) )
Moffat β : r_local = α·sqrt( ( F(β−1)/(π α² k σ_bg) )^(1/β) − 1 ),  α = FWHM/(2√(2^(1/β)−1))
r_i = clip( r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax )
  k = 0.1,  r_min = max(1.5 px, 0.75 FWHM_i),  rmax = 60 px（硬上界）
天空预算: 取最大 s∈(0,1] 使 n_qualified(s) ≥ 8 且 N_sky(s) ≥ 9216，否则 rc=1（显式拒）
```

**实测偏差**（`NOISE_MODEL.md:93`）：统一 60 px worst 6.93% / 逐星自适应 **1.11%** / 平均零权重像素占比 14.8% → **0**。

### 2.2 天光：两种口径与推荐

这是负责人最关心的一点。天光 `B` 同时是**信号的一部分**（它落进像素）与**噪声源**（泊松）。必须在定义上分开。

#### 口径 A（背景扣除，flux-type）— **推荐**

```text
信号:  F̂ = Σ_p w_p ( I_p − B̂_p ) ,      B̂_p 来自独立局部背景估计（不用源自身像素）
噪声:  σ_F⁻² = Pᵀ C⁻¹ P ,  C = diag(σ²_pix) ,  σ²_pix 中**保留**天光泊松项 B_p/G
SNR:   SNR_k(F_ref) = F_ref / σ_F,k                                                     (2.3)
```

性质：**加性天光平移不改变信号项**（`I_p − B̂_p` 对 `B → B + const` 不变），但天光变亮使 `σ_pix` 变大、SNR 如实下降。
这正是 `docs/plugins/algorithms_phase1/07_noise_snr.md:40-41` 的要求："信号项不被加性天光背景虚高……天光散粒噪声是真实噪声的一部分"。

#### 口径 B（背景作为噪声，power-ratio / 全局 SNR）— **不推荐作帧级参考**

```text
SNR_global² = ( Σ_p I_p )² / ( Σ_p σ²_pix )      或   σ²_signal / σ²_noise（全局尺度比）
```

性质：**随天光变亮而虚高**（分子含 `B`，分母只含其平方根量级的增长）。
`07_noise_snr.md:40` 明确："普通 SNR（全局信号方差/噪声方差，含天光背景）随天光变亮而虚高，不能作唯一帧级参考"；
`07_noise_snr.md:107`："帧级 SNR 无法计算 → fail-closed，**不得用受天光影响的普通 SNR 代替**"。
PixInsight 官方文档亦指出其标准 SNR（式[20] `σ²/σ_n²`）受背景梯度与天光正向影响（`docs/science/PSF_SIGNAL_WEIGHT.md:44`）。

#### 推荐与理由

| | 口径 A（扣除） | 口径 B（含背景） |
|---|---|---|
| 加性天光平移 | **不变** | 虚高 |
| 天光泊松噪声 | 如实进 σ | 部分混入分子 |
| 逆方差换算 `w = SNR²/F_ref²` | **严格成立** | **不成立**（功率比量纲） |
| 跨帧可比性 | 好（需公共 `F_ref`） | 差 |

**推荐 A**。附加硬约束（三条，缺一不可）：

1. `B̂_p` 必须是**独立局部**估计（不是全帧单一常数）。当前生产用**全帧裁剪中位数**（`star_detector.cpp:63`）
   在 5×5 窗口内扣除（`star_detector.cpp:144-151`）——**这是"全局背景"，不是文档要求的"独立局部背景"**
   （`07_noise_snr.md:51`）。梯度场下会留下残余基座，使 `F̂` 有百分之几的偏差。
2. `F_ref` 必须是**组内公共常数**，否则 `w = SNR²/F_ref² = a_f²/σ_f²` 的配对定理失效（`lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:34-41`）。
3. 天光的**泊松项**必须在 `σ²_pix` 中保留；不能因为"已经扣了背景"就把天光从噪声里也删掉。

#### 天光规避的完整处置表

| 天光进入的通道 | 错误处置 | 正确处置 |
|---|---|---|
| 落进像素值 → 信号 | 不扣背景，`F̂` 含 `B` ⇒ SNR 虚高 | 扣除**独立局部** `B̂_p` |
| 泊松涨落 → 噪声 | 扣背景时把方差也一起减掉 | **保留** `B_p/G` 在 `σ²_pix` |
| 空间梯度 → 伪信号 | 用全帧常数背景 | 用与 `σ_bg` 同尺度的局部背景（patch/网格） |
| 未分辨结构 → 被当成噪声 | 用未裁剪 MAD | 掩膜 + 5σ 裁剪（§2.1.2） |
| 天光面拟合参数不确定度 | 只传播值不传播方差 | `J_out C_θ J_outᵀ`（§4.3） |

### 2.3 帧级总 SNR 的定义与用途

```text
帧级 SNR（唯一帧级参考，写 HiPS 文件头）:
   SNR_k(F_ref) = F_ref · sqrt( W_info,k ) = F_ref / σ_F,k                              (2.4)
   W_info,k = a_k² P_kᵀ C_k⁻¹ P_k = 1/Var(F̂_k)
   白噪声特例: W_info,k = a_k² Σ_p P_k,p² / σ²_pix,k = a_k² / ( σ²_pix,k · A_NEA,k )
```

- **是"未加权的原始信噪比，不是权重"**（`07_noise_snr.md:39`）：它描述客观测量事实，不含任何为某次叠加服务的加权；
  权重由 Phase2 从 SNR 现场换算。
- **用途**：① HiPS 文件头的唯一帧级参考；② Phase2 权重链的回退路径 `w_k = SNR_k²/F_ref²`；③ 深度指标 `m5 = ZP − 2.5 log10(5 σ_F(ref))`。
- **不是**：不是 `median(SNR_F)`（那是分布摘要），不是 5σ 深度，不是 PSFSW（无量纲复合权重，`07_noise_snr.md:83` 明令不得混）。
- **标量降级门**（`07_noise_snr.md:82`）：仅当帧内 `W_psf(x,y)` 的鲁棒相对离散与系统趋势低于阈值才允许压为帧级标量；
  否则必须存 map/控制点/多项式/HEPix 并附 p05/p50/p95、最大系统偏差、覆盖与模型误差。**这是"稀疏层"的正式依据。**

### 2.4 稀疏帧内 SNR 层（帧级之外的第二个量）

```text
实际 SNR(x,y) = 帧级 SNR_k × 帧内相对因子 intra_k(x,y)                                 (2.5)
intra_k(x,y) = sqrt( W_info,k(x,y) / ⟨W_info,k⟩ )        （组内归一，无量纲，p50 = 1）
```

- 控制点存**未加权 SNR**（`07_noise_snr.md:70`），不是权重；
- 契约载体已存在：`contracts/schemas/unified/sparse_snr_layer.schema.json`，
  `control_points[{x, y, sparse_snr_value}]`，`role = intra_frame_reference`，
  `object_weight_capability = false`（"该层是参考层，不是 variance/ivar，不承载帧级权重"）。
- **本文的设计增量**：冻结 `sparse_snr_value` 的**数值语义**与**控制点值的支撑尺度**（§3.1）。
  现行 schema 只写"控制点 SNR 参考值"，未区分"绝对帧内 SNR"与"相对帧级因子"——这是必须冻结的口径缺口。

### 2.5 与现有实现的对照（核实结果）

**结论：文档冻结的噪声模型完全未运行；生产实际用的是另外两个更简单的估计器。**

| 量 | 文档（权威） | 生产实现 | 一致? |
|---|---|---|---|
| 空背景 σ | 掩膜 8×8 patch MAD + 5σ≤2 轮裁剪 + 逐星半径 + 饱和域 + 天空预算（`NOISE_MODEL.md:52-54,75-91`） | **整帧未裁剪 MAD**（`wrapper_phase1/noise_model.cpp:164-190`） | **否** |
| SNR 用的 σ_sky | 同上 | **整帧裁剪 RMS**（`star_detector.cpp:41-67`，无源掩膜）——**第三个**估计器 | **否** |
| patch 网格 + 控制点 | 8×8 patch，合格 patch variance → 控制点 | 生产 TU 中无 patch 循环 | **否** |
| 空间方差场 | `var(x,y) = a + b x + c y`，几何门 `λ_lo/λ_hi ≥ 1/16` | 无 | **否** |
| `variance_floor = 1e-12` | 冻结 | 标量上生效 | 是（仅标量） |
| `ivar` | **逐像素** `1/max(variance, floor)`，Phase2 权重来源 | **每帧一个标量** `1/max(σ²,1e-12)` | **否** |
| 读出噪声 / 源泊松 | `σ_i² = σ_sky² + max(F P_i,0)/G + (RN/G)²` | 仅在用户 JSON 显式给 `gain>0 && rn>0` 时（默认 0）⇒ **默认死支** | 部分 |
| 暗流 / 平场方差 | `V(y_p)={V(r)+V(b)+α²[V(d)+V(b)]+y_p²V(f)}/f²` | 噪声路径中**完全没有** | **否** |
| 天光扣除 | 独立**局部**背景 | 已扣，但是**全帧裁剪中位数** | 部分 |
| 天光作为噪声 | `σ_sky²` 进 `σ_i²` | 是 | 是 |
| 逐源 SNR | `SNR_F = F/σ_F`，`σ_F⁻² = Σ P_i²/σ_i²`（Horne 1986） | 同式（`snr_science.cpp:170-182`） | 是 |
| 信息权重 `W_psf = a²PᵀC⁻¹P` | 文档主式 | 实现存在（`information_weight.cpp`）但**不在生产闭包**；生产用解析 Moffat4 天光限近似 | **否（未接线）** |
| 帧级 SNR | 未加权原始通量型 SNR 写头 | 头里写的是**合成参考轮廓**的 `snr_reference.snr_f` | 部分 |
| `frame_snr` 字段 | 文档两处**互斥**（UNRESOLVED） | `p1_snr.json` 的 `frame_snr` 实为 **5σ 深度**（自述 "NOT a whole-frame scalar SNR"） | 冲突 |
| 稀疏 SNR 层 | 可选 HiPS 标准层 | 配置键 parser-only（`parser.cpp:320`），**无消费者** | **否** |
| 逐像素 variance 产品 | HiPS `variance`+`ivar`，`var_p=Σ v_j w_jp²/D_p²` | drizzle 只加 `"data"` block（`module_adapters.cpp:3962`）⇒ 不写 | **否** |
| 饱和域 | `x ≥ saturation_level` 剔除；未提供电平须写显式降级声明 | `p1_op_noise` 中无；声明串只在 legacy orchestrator | **否** |

**`CHK-ALGO-WIRING` 核实**（任务书要求）：

- 生产目标 `astrocs_phase1_noise`（`CMakeLists.txt:620-623`）编入 **3 个 TU**：
  `wrapper_phase1/noise_model.cpp`、`wrapper_phase1/snr_frame_science.cpp`、`cpp/src/snr_science.cpp`；
- `snr_noise_model_v1*` 的定义处 `cpp/src/noise_model.cpp` **不在其中**（它只属于被 `CMakeLists.txt:240-245` 注释掉的 `astrocs_p1_noise` 与测试局部目标）；
- **任务书原判据需两处订正**：① "只编 wrapper_phase1" **不准确**——还编了 `cpp/src/snr_science.cpp`（P5 权威 Horne SNR 实现）；
  ② 链接链为 `astrocs` → `astrocs_cli_runtime` → `astrocs_module_adapters` → `astrocs_phase1_session` → `astrocs_phase1_noise`（`CMakeLists.txt:705-706, 828-857, 871-874, 793-801`）。
- **差距量级（实测，EXP-3 Part A）**：同一帧内，`p1_snr.json.frames[].variance` 隐含的 σ（整帧 MAD）
  与 `p1_sources.json.frames[].noise_sigma`（SNR 实际使用的裁剪 RMS）之比中位数 **1.3127**（8 帧范围 1.0939–1.5731），
  **方差比中位数 1.7676**。即：**同一个产品里的两个"噪声"互相矛盾约 1.3×（σ）/ 1.8×（方差）**，
  而 `weight_mode=2` 的 tier-3 权重恰恰读的是前者。

### 2.6 单帧 SNR 的可验证判据

| # | 判据 | 正例（绿） | 负例（必须红） |
|---|---|---|---|
| C1.1 | 合成注入-回收：注入已知 `(F, FWHM, σ_bg)` 的点源，`σ̂_F` 与解析 `σ_sky/√(ΣP²)` 一致 | rtol ≤ 5% | 把 `FWHM=2.3548σ`（高斯矩）当 Moffat4 FWHM 用 ⇒ 系统性偏离 1.914×（见下） |
| C1.2 | 与 `photutils`/`SEP` 孔径 SNR 对拍 | 同孔径同背景估计下 rtol ≤ 5% | 用不同背景估计器 ⇒ 必须显示差异而不是"通过" |
| C1.3 | **加性天光平移不改变信号项**（`07_noise_snr.md:114`） | 注入 `B += const`，`F̂` 变化 < 1e-9 相对 | 用全帧常数背景 + 梯度场 ⇒ `F̂` 随注入常数漂移 ⇒ 红 |
| C1.4 | **天光散粒噪声增强时 SNR 按理论下降** | 注入额外泊松噪声，`SNR ∝ 1/σ` | 把天光从噪声里也扣掉 ⇒ SNR 不降 ⇒ 红 |
| C1.5 | 常量场不变量 | 常数输入 ⇒ `σ_bg = 0`、退化路径、不产生伪梯度 | 未裁剪 MAD 在含星帧上给出非零伪梯度 ⇒ 红 |
| C1.6 | 源污染门 | `|σ̂_bg/σ_bg − 1| ≤ 2%`（12 seeds） | 固定 2 px 半径 + `F_max=10⁶` ⇒ 必须检出 > 2% 偏差 |
| C1.7 | 饱和域门 | 提供电平后帧平均 `ivar` ≥ 3× 未过滤臂 | 电平未提供且含亮星饱和核 ⇒ `sigma_bg_global` 两臂差 0%（**静默**）⇒ 缺陷只在逐像素权重场可见 |
| C1.8 | **帧内一致性**（本文新增，直击 §2.5 的实测矛盾） | `sqrt(variance_scalar)` 与 `noise_sigma` 之比 ≤ 1.05 | 当前实测 1.31 ⇒ **必红** |
| C1.9 | **FWHM 口径一致性**（本文新增） | `sources[].fwhm_px` 与 `snr_science` 所用的 profile FWHM 同口径 | 高斯矩 `2.3548σ` 喂给 `FWHM = 1.230310σ` 的 Moffat4 ⇒ `σ_PSF` 高估 1.914× ⇒ `ΣP²` 偏小 ⇒ `σ_F` 偏大 ⇒ **SNR_F 系统性偏低** ⇒ 红 |

> **待定（诚实登记）**：C1.9 的**绝对偏差量级**未在本轮复算（需要 NumPy oracle 对 Moffat4 轮廓积分）；
> 判定方法：用 `snr_science.cpp` 的轮廓生成器对同一 `(F, FWHM)` 分别取 `σ_PSF = FWHM/1.230310` 与 `σ_PSF = FWHM/2.3548`，
> 比较 `Σ_p P_p²` 与 `σ_F`；预期比值可由 `A_NEA` 的解析式直接给出。

---

<!--PART2-->
