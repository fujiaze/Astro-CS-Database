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

## 3 ② 稀疏 vs 稠密：控制点 SNR 层的设计

### 3.1 问题陈述与判据先行

负责人指出：**不能一帧只有一个总 PSF SNR，必须有不同区域的稀疏 SNR 加一个总 SNR，并由 Phase2 重建出稠密 SNR**。
本文把该要求形式化为**一个可判定问题**，而不是一个默认要做的优化：

> **判据 SP-0（先行冻结）**：稀疏 SNR 层**仅当**它对最终叠加的加权效率增益大于其重建误差带来的损失时才启用。
> 增益与损失都由同一个量度量（见 §3.6）：`Var_p(δ)`，`δ_f = ŵ_f/w_f − 1`。

**为什么必须判据先行**：由 §4 的结论 2，叠加估计量的效率损失是 `1 + Var_p(δ)`，其中 `Var_p` 是**逐像素对帧的加权方差**。
若所有帧在某像素的相对权重相同（`δ_f` 与 f 无关），则 `Var_p(δ) = 0`，**空间权重完全不带来增益也不带来损失**。
实测（EXP-3 Part B，真实 4 帧同 tile）：帧级标量权重的效率损失仅 **1.0013（σ 惩罚 0.063%）**，
而 64 px 稀疏重建反而带来 **1.068（σ 惩罚 3.3%）**的损失。**在该数据集上，稀疏层是净亏的。**

### 3.2 控制点布局：复用 Phase-2 UPM 控制网格（而非 Phase-1 patch 网格）

候选布局（两个都已存在于代码库中）：

| 布局 | 来源 | 间距 Δ | 每 4096² 帧控制点数 | 与实测相关长度 ℓ≈48 px 之比 |
|---|---|---|---|---|
| Phase-1 噪声 patch 网格 8×8 | `docs/science/NOISE_MODEL.md:52` | **512 px** | 64 | Δ/ℓ ≈ **10.7** |
| Phase-2 UPM 控制网格 8×8 / 512² tile | `docs/algorithms/PHASE2_SAMPLER.md:123` | **64 px** | 4096 | Δ/ℓ ≈ **1.33** |

**设计决策 D2（推荐）**：稀疏 SNR 层的控制点**复用 Phase-2 UPM 的 8×8/tile 控制网格**，即 `cell_side = 64` leaf px，
`n_controls = 64 × n_union_tiles`（含空覆盖占位，`DATA_SEMANTICS.md:1577`），`control_id` 沿用既有编码。

**理由（三条，均可独立复核）**：

1. **采样定理判据**（Shannon 1949, DOI 10.1109/JRPROC.1949.232969）：控制点间距必须细于场中最小尺度的一半。
   实测 SNR 场的相关长度 `ℓ ≈ 48 px`（EXP-2，SE 变差函数拟合；变差函数 `γ(32)=0.0124, γ(96)=0.0224, sill=0.0296`），
   故 `Δ = 64 px` 满足 `Δ ≈ 1.33 ℓ` 的**近临界**条件，而 `Δ = 512 px` 严重欠采样。
2. **重建误差的解析标度**（EXP-4，见 §3.5）：kriging 误差由 `Δ/ℓ` 单参数控制。`Δ/ℓ ≤ 1` 时误差 `≤ 0.13σ`；
   `Δ/ℓ ≥ 3` 时误差 `→ 1.0σ`，即**控制点不含信息**。512 px 网格落在后者。
3. **系统复用**：Phase-2 已经为每个控制点产出 `control_variance` / `control_ivar` / `uncertainty`（`p2_samples.json` 实测字段齐全），
   并已有 `target_order`、`nside = 2^(target_order+9)`、`leaf_ipix`、`ra_deg/dec_deg` 全套几何（`DATA_SEMANTICS.md:1074-1077`）。
   把 SNR 层挂到同一网格**不需要新的空间分解、不需要新的几何哈希**，且重建算子可直接复用 v6 的 `bilinear_regular_grid_v1`
   （`lib/algorithms/integration/v6/src/weight_chain.cpp`）。

**不推荐的替代**：

- 用 Phase-1 的 8×8 patch 网格：间距 512 px 已超出信息极限（EXP-4），且该网格在生产 TU 中**根本未运行**（§2.5）。
- 用源检测副产品（`p1_snr.json` 的 `sources[].snr_f`，实测 86,717 个源）：密度随天区剧变（密集区极密、空白天区为零），
  **不是受控布局**，无法保证覆盖，也无法给出重建误差的解析控制。可作为**独立交叉验证**（见 §3.8 判据 C3.4），不作主载体。
- 自适应加密：本项目**没有任何**自适应细化的文档或实现（`docs/**` 全文检索只找到 `nside=512` 的三处 MC 相关数），
  且自适应会破坏「重建误差可解析给出」这一性质。**本文不设计自适应**，登记为未来工作。

### 3.3 控制点存什么值（口径必须冻结）

现行 schema（`contracts/schemas/unified/sparse_snr_layer.schema.json`）只写 `control_points[{x, y, sparse_snr_value}]` 与
`role = "intra_frame_reference"`，**未冻结数值语义**。本文冻结为：

```text
控制点 c 的值（三选一，必须显式声明用的是哪一个）:
  (a) 绝对帧内通量型 SNR  :  SNR_c = F_ref / σ_F,c
  (b) 相对帧级因子（推荐）:  rho_c  = SNR_c / SNR_frame = sqrt( W_info,c / <W_info>_frame ),  p50(rho) = 1
  (c) 局部天光 SNR（仅诊断）: SNR_sky,c = B_c / σ_bg,c          —— 与帧级参考同量纲但语义不同，禁止混用
```

**推荐 (b)**，三条理由：

1. 它与既有权威的合成规则**逐字一致**：『实际 SNR = 帧级 × 帧内』（`docs/plugins/algorithms_phase1/07_noise_snr.md:75`）；
2. 它是**无量纲**的，组内归一后不受 `a_k`/零点口径影响，跨帧可直接比较与插值；
3. 帧级标量已经承载绝对尺度，稀疏层只需承载**空间形状**，把绝对与相对分开可以独立检验。

**控制点值的支撑尺度（本文新增的冻结项）**：每个控制点的值定义为该 `cell_side × cell_side`（64×64 leaf px）单元内
**合格空背景像素的稳健 MAD 方差**，再按 `W_info = a² ΣP² / σ²_pix` 换算为 SNR。
即：**控制点值代表一个 64×64 patch 的平均 SNR**，不是该点的瞬时值。这条必须在 schema 里显式写出，
否则重建算子会把「点值」误当「点采样」，在 `Δ ≈ ℓ` 时引入不可控的偏差。

**必须携带的伴随量**（否则重建误差不可算）：

```text
per control point:  { x, y, rho_c, n_sky, sigma_rho, quality_flags, cell_side_px }
per frame:          { frame_id, snr_frame, F_ref, a_k, fwhm_px, correlation_length_px, estimator_id }
```

`sigma_rho` 由 `SE(σ̂_bg)/σ_bg ≈ 1.44/sqrt(N_sky)`（`NOISE_MODEL.md:5a` 冻结式）与 `N_sky ≥ 9216` 的预算共同给出；
`correlation_length_px` 是**重建算子选择与误差预测的必需输入**，不能事后估计。

### 3.4 重建算子

| 算子 | 现有实现 | 优点 | 缺点 | 结论 |
|---|---|---|---|---|
| 双线性 | `bilinear_regular_grid_v1`（v6 `weight_chain.cpp`） | 已实现、O(1)、保形（凸组合） | `Δ ≥ 2ℓ` 时**比常数还差**（EXP-4：`bil/krig` 比值 1.12–1.18，SE 核；2.33 @ Δ=ℓ） | 作**基线**，不作推荐 |
| 最近邻 | `nearest_control_point_v1`（需显式 `max_radius_px>0`） | 严格保形、无过冲 | 阶梯伪影；`Δ` 尺度上误差最大 | 仅用于 `max_radius_px` 受限的诊断 |
| 平面拟合 | 生产 UPM 的空间场模型 `var = a + bx + cy` | 与 UPM 一致、极稳健 | 只吃 3 个自由度，实测 RMSE 与双线性相当或略差（EXP-2） | 保留为 UPM 内部模型 |
| **kriging / GP** | 无（需新增） | **给出闭式预测方差** `K** − K* (K+σ_n²I)⁻¹ K*ᵀ`（Rasmussen & Williams 2006），**正是 `PΣPᵀ` 结构**；`Δ ≲ ℓ` 时误差比双线性小 2.3× | 需核超参数（由变差函数拟合）；`O(N³)`（本项目 N=4096 可接受） | **推荐** |
| P-spline | 无 | 线性光滑器、协方差闭式、光滑度可调（Eilers & Marx 1996） | 需选基与惩罚；球面需注意坐标畸变 | 备选（若需多尺度） |

**设计决策 D3（推荐）**：主算子用 **GP/kriging**，核由本帧实测变差函数拟合（指数或 SE 核 + nugget = `sigma_rho²`），
并**必须**返回逐像素预测方差 `Var_recon(p)`，该方差进入 §4 的权重分母。双线性保留为**基线算子**用于回归测试。

**关键实现约束（本轮发现的真实 bug 级细节）**：kriging 的 nugget **必须**取控制点自身的测量方差 `sigma_rho²`，
不能取数值条件化用的小量。EXP-4 首次运行时用 `1e-10·I` 作 nugget，导致 `1 − kᵀw ≈ 0`、预测方差恒为 0，
`bil/krig` 比值虚高到 2437–111803。改用**物理 nugget** 后结果正常（见 §3.5 表）。**这是必须写进实现判据的陷阱。**

### 3.5 精度损失量化

#### 3.5.1 解析标度律（EXP-4，合成真值，能红能绿）

对已知核的平稳高斯场，在 `n×n` 控制点网格中心做 kriging，误差标准差（单位：场的 σ）与 `Δ/ℓ` 的关系：

| Δ/ℓ | kriging σ_err（5×5） | 双线性 σ_err | 双线性/kriging | 网格不可分辨的功率占比 |
|---|---|---|---|---|
| 0.25 | 0.0002 | 0.0218 | 124.0 | 0.000 |
| 0.50 | 0.0054 | 0.0844 | 15.6 | 0.000 |
| 1.00 | **0.1271** | 0.2960 | 2.33 | 0.000 |
| 1.50 | 0.4581 | 0.5469 | 1.19 | 0.000 |
| 2.00 | 0.7550 | 0.7658 | 1.01 | 0.000 |
| 3.00 | 0.9780 | 1.0221 | 1.05 | 0.013 |
| 4.00 | 0.9993 | 1.1016 | 1.10 | 0.088 |
| 8.00 | 1.0000 | 1.1180 | 1.12 | 0.544 |

（SE 核，`ℓ = 1`；OU 核同表给出 `krig(Δ=ℓ) = 0.713`、`bil/krig ≈ 1.00`——**粗糙场下 kriging 相对双线性没有优势**，
这是一个重要的负面结果：**kriging 的收益依赖场的可微性**，必须先测变差函数再选算子。）

**三条可写进论文的结论**：

1. `Δ/ℓ ≤ 1` 是稀疏层有效的**必要条件**；`Δ/ℓ ≥ 3` 时 kriging 误差 `→ 1.0σ`，等价于**没有控制点**；
2. 双线性在 `Δ ≥ 2ℓ` 时误差 **超过 1.0σ**——即**比直接用帧级常数更差**（外推梯度导致）；
3. 网格不可分辨的功率占比随 `Δ/ℓ` 上升（`0.088 @ 4ℓ`、`0.544 @ 8ℓ`），这是**稀疏化不可消除的精度上限**，
   与算子无关。**这条给出了「稀疏 SNR 层能到多准」的诚实上界。**

#### 3.5.2 真实数据实测（EXP-2 / EXP-3）

数据：`run/RELEASE-02/L4-rebuild/norm/t2_m1_red`（M42_M1_T2 300S Red，4096²，真实校准帧）；
32 px patch → 128×128 网格；评价窗口为中心 1024² px（patch 索引 [48,80)）。

真值统计：`σ_pix` 中位数 **13.919 ADU**，p95/p05 = **1.372**；SNR 场（∝ 1/(σ·FWHM)）对数标准差 **0.1892**（窗口内 0.1367）；
FWHM 场近似常数 3.310 px（**91.5% 的 32 px patch 少于 3 颗星** ⇒ 只有全局中位数可用）；
真值估计量自身的标准误中位数 4.5%。

| 控制点间距 | 控制点数 | RMSE(log SNR) | 双线性 | 平面拟合（生产模型） | 相对帧级标量的改善 |
|---|---|---|---|---|---|
| 64 px | 4096 | — | **0.12307** | 0.14669 | — |
| 128 px | 1024 | — | 0.14516 | 0.14616 | — |
| 256 px | 256 | — | 0.13974 | 0.14538 | — |
| 512 px | 64 | — | 0.14248 | 0.14297 | — |
| 1024 px | 16 | — | 0.16220 | 0.14893 | — |
| **帧级标量（无空间信息）** | 1 | — | **0.13666** | — | **基准** |
| 打乱控制点（负例） | 4096 | — | **0.16795** | — | 必须显著变差 ✓ |

**诚实结论**：在**这一帧**上，除 64 px 外所有稀疏重建的 RMSE **与帧级标量同量级**（0.1367 vs 0.1397–0.1622），
即空间信息几乎没被恢复。原因是 `Δ/ℓ ≈ 1.33` 已接近临界，且场的**小尺度（< 64 px）成分占比不小**。
64 px 时 RMSE 降到 0.123（相对帧级标量改善约 10%），这是**唯一**显示正收益的配置——与 §3.2 的 D2 一致。

**打乱负例通过**（0.1680 > 0.1367），说明度量确实对空间信息敏感，不是自证。

#### 3.5.3 权重效率的实测（EXP-3 Part B，真实 4 帧）

tile `t2_m2_red`（M42_M2_T2 300S Red，4 帧完全重叠）。帧间 σ 散差仅 **1.0577**。

| 间距 | 控制点数 | RMSE(log σ) | **实测效率惩罚** `1+Var_p(δ)` | 最坏情况 `1+4·RMSE²` | σ 惩罚 |
|---|---|---|---|---|---|
| 64 px | 4096 | 0.09223 | **1.0678** | 1.0340 | 3.33% |
| 128 px | 1024 | 0.11240 | 1.1611 | 1.0505 | 7.76% |
| 256 px | 256 | 0.13160 | 1.3117 | 1.0693 | 14.53% |
| 512 px | 64 | 0.13463 | 1.2851 | 1.0725 | 13.36% |
| 1024 px | 16 | 0.23041 | 1.2122 | 1.2123 | 10.10% |
| **帧级标量** | 1 | — | **1.0013** | — | **0.063%** |

**这是本文最重要的定量结论之一**：当各帧的空间 SNR 形状几乎相同时（本 tile 的 4 帧同望远镜、同曝光、同滤光片、相近天况），
帧级标量权重已经**最优到 0.06%**，任何稀疏重建引入的**帧间独立误差**都会让结果变差。

### 3.6 增益判据（把 §3.1 的 SP-0 变成可算的式子）

```text
记真权重 w_f(p)，帧级常数近似 w̄_f = <w_f>_p，稀疏重建近似 ŵ_f(p)。
δ_f(p) = ŵ_f(p)/w_f(p) − 1 ,   p_f(p) = w_f(p) / Σ_g w_g(p)
效率惩罚:  Var_approx/Var_opt = 1 + Var_p(δ) + O(δ³)                       (3.1)

两个来源相加（近似独立）:
  Var_p(δ)  ≈  Var_p(δ_空间省略)  +  Var_p(δ_重建误差)                     (3.2)
  δ_空间省略 = w̄_f/w_f − 1     （帧级标量的误差，只由帧间相对权重差异驱动）
  δ_重建误差 ≈ −2 ε_f ,  ε_f = Δlog SNR 的重建误差（帧间独立 ⇒ Var_p 放大约 4(1−1/F) 倍）

判据 SP-0（可算形式）:  启用稀疏层  ⟺  Var_p(δ_空间省略) > Var_p(δ_重建误差)   (3.3)
```

**式 (3.1) 已由 EXP-1 独立验证**：解析式与 Monte Carlo 在 5 位小数一致（`δ_rms = 0.20` 时 ratio 1.03185 对 1.03185）；
**共模权重误差的 ratio 精确为 1.000000**（`Var_p = 4.5e-34`，机器零），而同等幅度的帧间独立误差给出 1.03172。
**这是「稀疏层可以只做相对、共模尺度误差无害」的定量依据，也是本设计能被接受的前提。**

**判据 SP-0 的物理读法**：稀疏 SNR 层的价值**不在**「场有结构」，而在「**各帧的结构不同**」。
不同天况/不同月相/不同气团/不同滤光片/不同 PSF 的帧混合时 `δ_空间省略` 大，稀疏层有正收益；
同批同条件帧混合时 `δ_空间省略 ≈ 0`，稀疏层是净亏。**这解释了为什么不能无条件启用它。**

### 3.7 数据产品 schema

在既有 `sparse_snr_layer.schema.json` 基础上**只增不改**（`additionalProperties` 已允许）：

```text
sparse_snr_layer:
  frame_id            : string
  role                : "intra_frame_reference"        # 既有，不改
  estimator_id        : "snr_sparse_kriging_v1"        # 新增：算子+核+参数版本
  value_semantics     : "relative_to_frame_snr"        # 新增：(a)/(b)/(c) 三选一，冻结
  support_scale_px    : 64                             # 新增：控制点值代表的 patch 边长（leaf px）
  grid                : { scheme: "upm_control_grid_v1", cell_side_px: 64, grid_per_tile: 8,
                          target_order: 9, nside: 1024, ordering: "NESTED" }   # 新增
  correlation_length_px : 48.0                         # 新增：实测，重建误差预测的必需输入
  control_points      : [ { x, y, sparse_snr_value, n_sky, sigma_value, quality_flags } ]
  density             : { points_per_deg2: <computed> } # 既有可选键
  reconstruction      : { operator: "kriging_gp_v1", kernel: "se", nugget_source: "sigma_value",
                          returns_variance: true }        # 新增：必须声明是否返回方差
```

**硬约束**：

- `object_weight_capability` **保持 false**（该层是参考层，不是 variance/ivar，不承载帧级权重）；
- 稀疏层的值**禁止**写入 `ivar`/`variance`/`W_info`（`docs/science/PSF_SIGNAL_WEIGHT.md:57` 的同类红线）；
- 无控制点的 tile **必须**写占位节点（`n_controls` 含空覆盖占位，与 `DATA_SEMANTICS.md:1577` 一致），**禁止**用插值伪造覆盖；
- 帧级 SNR 缺失时稀疏层**必须**整体 fail-closed，**禁止**用受天光影响的普通 SNR 顶替（`07_noise_snr.md:107`）。

### 3.8 判据与能红能绿的负例

| # | 判据 | 正例（绿） | 负例（必须红） |
|---|---|---|---|
| C3.1 | **采样充分性**：实测 `correlation_length_px` 与 `Δ` 满足 `Δ ≤ 2ℓ` 才允许启用 | 64 px 网格 + ℓ=48 ⇒ 通过 | 用 512 px patch 网格 ⇒ `Δ/ℓ=10.7` ⇒ 判据拒 ⇒ **红**（当前设计的默认值必须被拒） |
| C3.2 | **kriging nugget 必须物理** | 用 `sigma_value²` ⇒ 预测方差 ∈ (0, σ²] | 用 `1e-10·I` ⇒ 预测方差恒 0、`bil/krig` 达 1e5 量级 ⇒ **红**（EXP-4 首轮真实发生） |
| C3.3 | **打乱控制点负例** | 打乱后 RMSE 显著变差（0.1680 vs 0.1367） | 打乱后 RMSE 不变 ⇒ 度量对空间信息不敏感 ⇒ 红 |
| C3.4 | **与源检测副产品交叉验证** | kriging 重建的 SNR 场与 `p1_snr.json` 的 `sources[].snr_f`（86,717 点）在同位置一致（Pearson(log) > 0.5） | 实测仅 **0.276**（散度 0.369 dex）⇒ **当前为红**，说明两条路径测的不是同一个量（源 SNR vs 局部噪声 SNR）——**必须登记，不得掩盖** |
| C3.5 | **增益判据 SP-0** | 在帧间权重差异大的数据集上 `Var_p(δ_空间省略) > Var_p(δ_重建误差)` | 在同批同条件 4 帧上 `Var_p(δ_空间省略) ≈ 0` ⇒ 判据拒 ⇒ **红**（EXP-3 实测） |
| C3.6 | **共模不变性** | 给整帧乘常数 `c`，`δ` 不变、`Var_p(δ)` 不变 | 共模缩放改变 `Var_p(δ)` ⇒ 归一化实现错 ⇒ 红 |
| C3.7 | **占位节点** | 空覆盖 tile 仍产出 64 个占位控制点、`n_controls` 与几何一致 | 用插值补出非空值 ⇒ 红 |
| C3.8 | **方差单调性** | 控制点数从 16→4096 单调降低 RMSE（在 `Δ > ℓ` 区间） | RMSE 非单调且无解释 ⇒ 红 |

> **C3.4 是本文最需要负责人裁决的一条**：`p1_snr.json` 的逐源 `snr_f` 与局部空背景噪声 SNR **不是同一个量**
> （前者含源通量与 PSF 形状，后者只含局部噪声）。二者相关只有 0.276 说明它们**不能互相替代**。
> 判定方法（待定项）：把逐源 SNR 除以该源的 `√(ΣP²)/σ_pix` 因子后再比较，若相关性显著上升则确认差异来自口径；
> 否则说明两者之一有实现缺陷。**本轮未做，登记为待定。**

---

## 4 ③ Phase 2：SNR 如何传播到标准面模型

### 4.1 输入与数据流

```text
输入:
  逐帧 p1_snr.json    : { variance, ivar, sigma, frame_snr, snr_reference, sources[] }
  逐帧 p1_sources.json: { noise_sigma, background, psf_params[], sources[] }
  逐帧 HiPS            : signal/, support/（variance/ivar 当前未写，见 §2.5）
  稀疏 SNR 层（§3）    : control_points[{x,y,rho_c,sigma_value}]

Phase2 内部:
  coverage  →  target_order = min(f.max_leaf_order) ; nside = 2^(target_order+9)
  sampling  →  8x8 control/tile ; control_variance = k_corr*(pi/2)*sigma_bg^2/N_retained
  UPM 拟合  →  theta_hat 与 C_theta = (J^T W J)^-1
  UPM 施加  →  calibrated = raw - C_f(p) ;  (目标态) 还须 /g_k
  reject    →  sigma_eff^2 = sigma_phase1^2 + J_out C_theta J_out^T
  integrate →  w_k(p) = 1/Var(corrected_k(p)) ; ivar_mosaic = sum_k w_k
```

### 4.2 UPM 拟合中的权重

**冻结目标模型**（`docs/plugins/algorithms_phase2/11_upm.md:25-28, 57-60`）：

```text
y_k(x) = g_k · s(x) + b_k(x) + eps_k(x) ,   b_k(x) = B_ref(x) + delta_k(x)
min_{s, g, b}  Σ_k Σ_i  w_ki · [ y_k(x_i) − g_k·s(x_i) − b_k(x_i) ]²
w_ki = 1/σ²_ki ∝ SNR_ki²                                                     (4.1)
```

**生产冻结的加性模型**（`docs/science/PHASE2_UPM.md:47-75`）：

```text
calibrated_f(p) = raw_f(p) − C_f(p) ,   C_f(p) = bilinear(8x8 control cell, theta_f)
w_UPM    = quality_factor · control_reliability · control_ivar
w_cell   = w_UPM / Σ_cell(w_UPM) · control_reliability        （份额式，无量纲）
control_ivar = 1 / control_variance
control_variance = k_corr · (π/2) · σ_bg² / N_retained ,   k_corr 冻结 1.4，定义域 1 ≤ k_corr
```

**两式的关系（本文给出的桥）**：

```text
把 (4.1) 的 w_ki 取到控制点尺度：每个控制点的 value = 该 cell 的稳健中位数 y_ik，
其估计量方差即 control_variance ⇒  w_ki → 1/control_variance = control_ivar 。
因此  w_cell 的『份额式』与 w_ki 的『绝对式』差一个 cell 内的归一化因子 Σ_cell w_UPM 。
两者在 λs = 0（无光滑惩罚）时给出同一估计量；λs > 0 时份额式**丢弃跨 control 精度**，
改变估计量（PHASE2_UPM.md:93 已登记）。
```

**设计决策 D4（推荐）**：**保留份额式**（它是生产冻结式，改动需走 SCI 变更 claim），
但**必须**在 `p2_samples.json` 里同时保留绝对式所需的 `control_variance`/`control_ivar`（**已经保留**，实测字段齐全），
并在 UPM 输出里记录 `weight_form: "share" | "absolute"`，使两种口径可被独立复算。

**天光/背景在此处的角色**：`control_variance ∝ σ_bg²`，即**天光越亮、控制点权重越低**。
这是天光影响传播到 Phase2 的**主通道**（不是通过信号项）。这条必须写明，因为它是「规避天光影响」的正面回答：
**天光不进信号，只通过 σ_bg 降低权重，且在控制点尺度上被空间分辨。**

### 4.3 参数协方差 `C_θ`

```text
J = ∂r/∂θ |_θ̂ ,  r_i = y_i − model_i(θ) ,  W = diag(w_i)
C_θ = (Jᵀ W J)⁻¹                                                             (4.2)
```

**来源核查（§8 论断 2）**：该式是标准结果，可引 **GSL 参考手册**（Nonlinear Least-Squares Fitting → Covariance matrix of best fit parameters 小节，已实页核对节标题存在）
与 **Hogg, Bovy & Lang 2010 (arXiv:1008.4686)**。**三条必须一并声明的限制**：

1. 它要求 `W = C_d⁻¹` 且 `C_d` **已知到只差一个尺度**。本项目只有**相对**方差（MAD 估计），
   故 `C_θ` 的**绝对尺度不被该式确定**。**禁止**用 reduced-χ² 重标定（**Andrae et al. 2010, arXiv:1012.3754**：
   『The number of degrees of freedom can only be estimated for linear models. Concerning nonlinear models, the number of degrees of freedom is unknown』）。
   正确做法：把 `C_θ` 的绝对尺度接到**已知的物理噪声尺度**上（即 §2.1 的 `σ²_pix` 模型项），而不是接到拟合残差上。
2. 它是**局部线性化**结果（在最优点取 Jacobian），模型约束差或参数退化时**低估**方差。
3. `θ` 的 gauge 自由度（连通分量零锚，`zero_anchor_weight = 1e-3`）使 `JᵀWJ` 近奇异；
   **必须**在求逆前施加 gauge 约束（或使用伪逆），否则 `C_θ` 含虚假的巨大方差。

**设计决策 D5（推荐）**：`C_θ` 用 **Cholesky + gauge 投影** 求逆，输出
`C_θ` 的**对角**（逐参数方差）与**每帧、每控制点的预测方差** `diag(J_out C_θ J_outᵀ)`，
**不输出稠密 `C_θ`**（维度 `n_frames × n_controls` 的平方，不可存）。
预测方差按 tile 分块写出，与 `p2_samples.json` 的 tile 布局一致。

### 4.4 `J_out` 的分量

`J_out = ∂(被改正量)/∂θ`，对每个像素 p、每个参数分量：

```text
加性场:      ∂C_f(p)/∂θ_f,cell = 双线性基函数在 p 处的值（4 个非零，权重和为 1）
天光面:      ∂δ_k(p)/∂(样条系数) = 张量积 B 样条基（production: spline_degree=3, node_spacing_deg=1.0）
             ∂b_k(p)/∂(逐帧多项式系数) = 1, x, y, …（order 1）
乘性响应:    ∂[g_k·s(p)]/∂g_k = s(p)          ← 与 s 的估计耦合，需联合 Jacobian
             ∂[g_k·s(p)]/∂s(p') = g_k δ_{pp'}   ← 对角块
参考天光:    ∂b_k(p)/∂B_ref(p) = 1             ← 若 B_ref 也被拟合则非对角
```

**关键实现事实（已核）**：生产 UPM 是**纯加性**（`upm.cpp:4-6, 532`），`J_out` 只有第一行的双线性基；
`p2_upm_ma_*`（乘性+加性求解器）**已经**计算 `H = JᵀWJ`（`:2447-2449`）、`C_θ = VΛ⁻¹Vᵀ`（`:2467-2484`）、
并实现了 `p2_upm_ma_c_out`（`:2623-2643`），但**全仓零调用者**。

**设计决策 D6（推荐）**：`J_out C_θ J_outᵀ` 的**实现路径有两条**，本文推荐后者：

- (i) 把 `p2_upm_ma_*` 提升为生产（它已具备全部件，但乘性模型与生产冻结的纯加性模型冲突，需先裁决 §11 的 UNRESOLVED）；
- (ii) **（推荐）** 在现有生产 UPM 上加一个**只读的协方差出口**：拟合收敛后复用已有的 `J`/`W` 组装 `JᵀWJ`，
  gauge 投影后求逆得 `C_θ`，再用已存在的双线性基算 `J_out C_θ J_outᵀ` 的**对角**。
  这条路径**不改变估计量**（不动冻结的科学链），只增加一个输出，风险最小、可独立验收。

### 4.5 `C_out` 与 `÷g²`

```text
逐帧逐像素的被改正量方差（冻结目标式，docs/science/q2-snr-smooth 与 variance_propagation.h:71）：

  Var(corrected_k(p)) = [ σ²_y,k(p) + J_out(p) C_θ J_out(p)ᵀ ] / g_k²                  (4.3)

其中:
  σ²_y,k(p)          = 校准后帧的逐像素方差（§2.1 式 (2.2)）
  J_out C_θ J_outᵀ   = 拟合参数（天光面 / 加性场 / 乘性响应）的不确定度贡献
  g_k²               = 乘性响应的平方；g_k 同时把信号映射到公共尺度，故方差**必须除以 g²**
```

**为什么必须 `÷g²`（两条独立理由，均可复核）**：

1. **量纲/尺度一致性**：`corrected = raw/g`（或等价地 `g·s` 形式的改正）把 ADU 映射到公共通量尺度；
   线性变换 `x → x/g` 使方差变为 `Var(x)/g²`。若只改信号不改方差，权重与信号就**不在同一尺度**上，
   加权平均会被 `g` 系统性偏置。
2. **本项目已冻结该式**：`lib/algorithms/integration/v6/src/weight_chain.cpp:393` 的 `w *= g*g` 与
   `variance_propagation.h:71` 的 `Var = [PΣPᵀ + param_var]/g²` 都是同一约定的两处实现。

**当前生产事实（已核，必须登记）**：`g_k ≡ 1`。
`multiplicative_gain_applied` / `frame_gain` 被**读取**（`module_adapters.cpp:6344, 6368-6371`）但**全仓无写入者**；
上述两处 `÷g²` 都是**不可达代码**。⇒ **`÷g²` 今天是恒等式，但它一旦启用就是 1:1 传播进成品 SNR 的主导项。**

### 4.6 叠加权重：残差制造者方差（本设计的关键公式）

**不能**把 `Var(corrected_k(p))` 直接当叠加权重，因为 UPM 的参数是**从同一批帧估计**的，
被改正量与残差**相关**。正确的量是**残差制造者**的方差：

```text
corrected = (I − H) y = P y
Var(corrected_i) = Σ_j P_ij² σ_j²  +  [参数项]                                      (4.4)
P = I − H ,  H_kj = w_j / W_{−k}   （exclude-self：第 k 帧不参与自己的改正）
W_{−k} = Σ_{j≠k} w_j
```

**EXP-1 的解析与 MC 验证（N 帧等权）**：

```text
精确式:  Var_exact = σ² (1 − 1/N)² · [ 1 + (N−1)·(1/(N−1))² ] = σ² (N−1)/N
朴素式:  Var_naive = σ² + Var(ĝ) = σ² (1 + 1/N)
比值:    Var_naive / Var_exact = (N+1)/(N−1)
  N=4  → 1.6667   (MC 1.6667)
  N=8  → 1.2857   (MC 1.2857)
  N=16 → 1.1333   (MC 1.1333)
```

**⇒ 用朴素式会把方差高估 `(N+1)/(N−1)`，即 8 帧时高估 28.6%（σ 高估 13.4%）。**
`N = 2` 时该式发散，与「两帧无法互相改正」一致。这条是本设计对生产 `p2_corrected.json` 现有方差实现的**独立确认**：
实测该文件写的正是 exclude-self `PΣPᵀ`（`module_adapters.cpp:5091-5098, 5109-5175, 5642-5646`），**方向正确**。

### 4.7 权重优先链与 fail-closed

```text
integrate 的权重优先级（module_adapters.cpp:6708-6765，已核）:
  (1) 1/Var(corrected)          ← 当前不可达（param_covariance_included 硬编码 false）
  (2) w = SNR²/F_ref² · g_k²    ← 帧常数（weight_chain.cpp:387-399）
  (3) 逐样本 Phase1 ivar
  (4) 等权（weight_mode=1）
回退 fail-closed，带 closure token（:6395-6407）；legacy_allow_weight_fallback 是 no-op。
```

**两条权重链的关系（本文给出的桥，必须冻结）**：

```text
链 A（帧级，现役）: w_k = SNR_k(F_ref)² / F_ref² = a_k²/σ_F,k² = 1/σ_F,k²
链 B（逐像素，目标）: w_k(p) = 1/Var(corrected_k(p))

关系: 链 A 是链 B 在『PSF 与 σ_pix 均为帧常数』时的特例：
  σ_F,k² = σ²_pix,k · A_NEA,k / a_k²   ⇒   w_k = a_k²/(σ²_pix,k A_NEA,k)
  即链 B 把 σ²_pix,k 从帧常数换成 σ²_pix,k(p)，并加上参数项与 ÷g²。
因此 链 A → 链 B 是**严格加细**，不是替换；两条链必须给出一致的帧平均权重。
```

**设计决策 D7（推荐）**：积分权重用**链 B**，但**必须**同时输出链 A 的帧级值作为交叉校验；
两者的**帧平均**比值若偏离 1 超过 `sqrt(1 + Var_p(δ))` 的预测范围，判为不一致（红）。
这条把 §3.6 的效率判据变成了可运行的回归测试。

### 4.8 天光面与 SNR 的耦合

```text
生产天光面（独立模块，默认启用）:
  b_k(x) = B_ref(x) + δ_k(x)
  B_ref : 稀疏张量积 B 样条（production spline_degree 覆写为 3, node_spacing_deg=1.0,
          max_nodes=8192, roughness_penalty=1e-3）
  δ_k   : 逐帧 order-1 多项式
  只施加 δ_k（sky_plane_mode = "delta_to_B_ref"）
```

**对 SNR 的三条影响**（必须全部计入）：

1. **值**：天光均值被扣除 ⇒ 信号项不含天光（§2.2 口径 A）；
2. **方差**：`δ_k` 与 `B_ref` 的**系数不确定度**进入 `J_out C_θ J_outᵀ`（§4.5），即式 (4.3) 的分子第二项；
3. **权重**：`σ_bg` 越大的控制点 `control_ivar` 越小（§4.2），即天光通过**权重**影响标准面模型。

**设计决策 D8（推荐）**：天光面模块**必须**输出其系数协方差的**对角**（至少），否则式 (4.3) 的参数项不完整；
`node_spacing_deg = 1.0°` 对应约 `nside ≈ 64` 的角尺度——**远粗于** §3.2 推荐的 64 px 控制点间距，
因此天光面**不能**替代稀疏 SNR 层承载小尺度噪声结构（二者尺度差约 2 个数量级）。**这条必须写明，避免职责混淆。**

### 4.9 Phase 2 的可验证判据

| # | 判据 | 正例（绿） | 负例（必须红） |
|---|---|---|---|
| C4.1 | **残差制造者 vs 朴素式** | `Var` 比值符合 `(N−1)/N` 与 `(N+1)/(N−1)` 的解析预测 | 用 `σ² + Var(ĝ)` ⇒ N=8 时高估 28.6% ⇒ 红 |
| C4.2 | **`÷g²` 生效性** | 令 `g_k` 为已知常数 `c ≠ 1`，权重按 `1/c²` 缩放、成品 SNR 不变 | 只改信号不改方差 ⇒ 成品 SNR 随 `c` 漂移 ⇒ 红 |
| C4.3 | **`C_θ` gauge 不变性** | 改变零锚帧（同连通分量内）后 `J_out C_θ J_outᵀ` 的对角**不变** | 变化 ⇒ gauge 未正确投影 ⇒ 红 |
| C4.4 | **`C_θ` 正定性** | 特征值全 `≥ 0`（容许 1e-12 数值负） | 出现显著负特征值 ⇒ 红 |
| C4.5 | **绝对尺度可追溯** | `C_θ` 的绝对尺度接到 §2.1 的 `σ²_pix` 模型项，而非拟合残差 | 用 reduced-χ² 重标定 ⇒ **红**（Andrae et al. 2010 明确禁止用于非线性模型） |
| C4.6 | **`uncertainty_available` 真值条件** | 仅当 `逐像素噪声 ∧ 参数协方差 ∧ 全部帧 var 可用` 三者同时为真才置 true | 当前 `param_covariance_included` 硬编码 false（`module_adapters.cpp:5610`）⇒ 恒 false ⇒ **当前为红**，且这是**正确**的 fail-closed，**不得**为「让它亮」而放宽 |
| C4.7 | **两链一致性**（D7） | 链 A 与链 B 的帧平均权重比值在 `sqrt(1+Var_p(δ))` 预测范围内 | 偏离超出预测 ⇒ 红 |
| C4.8 | **reject 阈值含参数不确定度** | `σ_eff² = σ_phase1² + J_out C_θ J_outᵀ`（`12_rejection.md:22` 硬要求） | 用样本栈 MAD 代替 ⇒ 当前生产即如此（`module_adapters.cpp:6006, 6033`）⇒ **红**；且缺 `σ_eff` 时合同要求 fail-closed |
| C4.9 | **天光面尺度职责** | 天光面 node spacing ≥ 0.5°，稀疏 SNR 层 Δ = 64 px；二者尺度差 ≥ 50× | 用天光面承载小尺度噪声 ⇒ 红 |

---

## 5 ④ 成品 SNR 与误差预算

### 5.1 叠加公式

```text
精确逆方差权重:   ivar_mosaic(p) = W(p) = Σ_k w_k(p) ,   variance_mosaic(p) = 1/W(p)      (5.1)
一般权重:         variance_mosaic(p) = Σ_k w_k(p)² v_k(p) / W(p)²                       (5.2)
                  （v_k = Var(corrected_k)，W = Σ w_k；权重非纯逆方差时必须用此式）
信号:             signal(p) = Σ_k w_k(p) x_k(p) / W(p)
成品 SNR:         SNR_mosaic(p) = signal(p) / sqrt(variance_mosaic(p))                  (5.3)
```

（式 (5.1) 为 `docs/science/UNCERTAINTY_AND_COVARIANCE.md:59-60` 的冻结式；式 (5.2) 为同文件 `:61` 的一般式。）

### 5.2 与点源最优性的关系（必须声明的边界）

式 (5.1) 给出的是**逐像素**逆方差叠加，对**面亮度型**信号最优。
**点源**的最优统计量是 PSF 匹配滤波（Zackay & Ofek 2017, ApJ 836, 187, DOI 10.3847/1538-4357/836/2/187），
`docs/plugins/algorithms_phase2/13_integration.md:6` 已明文禁止宣称二者等价。
**本文的立场**：成品 `SNR_mosaic(p)` 是**逐像素**信噪比，**不是**点源探测信噪比；
点源 SNR 必须由 §5.4 的 PSF 加权形式另行给出，且**不得**用逐像素 SNR 冒充。

### 5.3 √N 律与实测增益（EXP-5）

在 49 帧真实数据上 `ASTROCS_FRAME_SNR` 的 max/min = **2.22×**（权重跨帧变化 4.9×）。取 8 帧子样本：

| 量 | 中位数 | p05 | p95 |
|---|---|---|---|
| 相对最佳单帧的 SNR 增益 | **2.647** | 2.526 | 2.731 |
| `sqrt(N) = 2.828` | — | — | — |
| 逆方差权重相对等权的增益 | **1.004** | 1.001 | 1.009 |

**两条结论**：

1. 增益 2.647 < `sqrt(8) = 2.828`，差距来自**帧间 SNR 不均**（不等权时 `Σw` 的调和性质）；
2. **在 8 帧内逆方差权重相对等权只增益 0.4%**——因为 8 帧是从 49 帧里取的、SNR 相近。
   这说明**帧级加权的收益取决于所选帧的 SNR 分布**，不能无条件宣称。

### 5.4 成品 SNR 的误差预算（EXP-5，两个不同问题分开）

#### (A) 叠加信号的随机误差（成品值多少）

由 §5.1，`σ_mosaic = 1/sqrt(Σ w_k)`；相对最佳单帧的增益见 §5.3。
**这是「成品有多好」，不是「报出的数有多准」。**

#### (B) 报出的 σ（等价于 SNR）**本身**的相对误差

```text
eps_tot² = eps_P1² + eps_sparse² + eps_theta² + eps_g² + eps_drizzle² + eps_master²      (5.4)
```

| 项 | 量级 | 证据 |
|---|---|---|
| `eps_P1` 估计量噪声 | 1.5%（`N_sky ≥ 9216` 的预算） | `NOISE_MODEL.md` §5a 冻结式 `SE(σ̂)/σ ≈ 1.44/√N_sky` |
| `eps_P1` **源污染偏差** | **31.3%**（σ）；方差 76.8% | **实测 EXP-3 Part A**：生产帧标量 `σ_MAD/σ_clippedRMS` 中位数 **1.3127**（8 帧范围 1.0939–1.5731），方差比 1.7676 |
| `eps_sparse` 稀疏重建 | 9.2%（64 px）～ 23.0%（1024 px） | 实测 EXP-2/EXP-3 `RMSE(log SNR)` |
| `eps_theta` UPM 参数 | **+3.4%（良态）～ +2200%（病态）** | `reports/RELEASE-02/unc-prop-audit.md`：`J_out C_θ J_outᵀ` = 0.46–333 ADU² vs 原始 σ² = 6.76 ADU² |
| `eps_g` 乘性归一化 | **今天结构上为 0**（`g ≡ 1`，无写入者）；一旦启用则 1:1 传播 | §4.5 |
| `eps_drizzle` 相关噪声 | **20.2%**（方差低估 36.3%） | `UNCERTAINTY_AND_COVARIANCE.md`：`≈1+0.75ρ`，`ρ = 0.19`（nside=512 MC） |
| `eps_master` 母版方差 | **未建模**（`CALIBRATION.md:236` 登记 UNRESOLVED） | 有限母版模型给出 `σ_master/σ_sky ~ 1/√N_master` |

**场景合成（EXP-5）**：

| 场景 | `eps_tot` |
|---|---|
| **当前生产**（帧标量 σ、无稀疏层、`g=1`、无 `C_θ`） | **131.3%** |
| 帧标量 σ 修好 + `C_θ` 传播（良态） | 3.7% |
| 帧标量 σ 修好 + `C_θ` 传播（病态） | 220.0% |
| 再 + 稀疏层 @64 px | 9.9% |
| 再 + 稀疏层 @512 px（Phase-1 patch 网格） | 14.0% |
| 再 + `g` 启用（1% 增益不确定度） | 10.0% |
| （`eps_drizzle = 20.2%` 在**每一行**都存在） | — |

**误差预算的三条结论**：

1. **当前主导项是帧标量 σ 的源污染偏差（131%），不是统计噪声（1.5%）**——修它比分母上任何其他工作都值钱；
2. **修好之后，下一个主导项是 drizzle 相关噪声（20.2%）**，它**只在把 variance 面接进权重链之后才显现**
   （今天 variance 面根本没写，所以这个误差是「隐藏」的）；
3. **`eps_theta` 的条件数依赖极强**（3.4% → 2200%），因此**必须**同时输出 `C_θ` 的条件数或最小特征值作为质量标记，
   否则「报出的 σ」在某些 tile 上完全不可信。

### 5.5 成品输出 schema 与真值条件

```text
目标态（DATA-P2-VAR-001 §30.1 / DATA_SEMANTICS.md:1108-1113 已冻结目标合同，实现不得先行）:
  variance/  : variance_mosaic(p)   [ADU²]   无信息 → 0.0（禁 NaN；NaN 保留给产品损坏）
  ivar/      : 1/variance           [1/ADU²] 无信息 → 0.0
  snr/       : signal/sqrt(variance)                        ← 本文新增建议，需走合同变更
  provenance : ASTROCS_UNCERTAINTY_AVAILABLE ∈ {"true","false"}
               ASTROCS_WEIGHT_MODE ∈ {0,1,2}
               ASTROCS_MODEL_HASH / ASTROCS_INPUT_MANIFEST_HASH
               ASTROCS_SNR_CHAIN_ID  ← 本文建议新增：链 A / 链 B / 链 A+B

uncertainty_available = true 的真值条件（三条同时成立，fail-closed）:
  (1) all_pixel_noise       : 逐帧 σ²_pix 面齐备（含 drizzle 传播）
  (2) param_cov_included    : J_out C_θ J_outᵀ 已计入
  (3) all_frames_var_ok     : 每帧 variance 面可用（无缺失、无 fallback）
```

**当前事实（已核，必须登记）**：`param_cov_included` 在 `module_adapters.cpp:5610` **硬编码为 false**，
故 `uncertainty_available` **恒 false**，积分 tier-1 权重 `1/Var(corrected)` 是**死代码**。
**这是正确的 fail-closed 行为**，不是缺陷；缺陷是**能力缺失**（`C_θ` 未接）。
**禁止**为「让灯变绿」而放宽该条件。

### 5.6 成品 SNR 的可验证判据

| # | 判据 | 正例（绿） | 负例（必须红） |
|---|---|---|---|
| C5.1 | **注入-回收（端到端）** | 合成已知 SNR 的源，成品 `SNR_mosaic` 与真值在 rtol ≤ 5% | 注入后 `SNR_mosaic` 不随注入噪声变化 ⇒ 红 |
| C5.2 | **`√N` 律的诚实性** | 等 SNR 帧时增益 `= √N`（rtol 1%）；不等 SNR 时**低于** `√N` 且与式 (5.1) 预测一致 | 宣称「总是 `√N`」⇒ 红 |
| C5.3 | **无信息像素语义** | 无覆盖写 `variance = 0 ∧ ivar = 0` | 写 NaN 或 `1/0 = Inf` ⇒ 红（`DATA_SEMANTICS.md:45-50` 明令） |
| C5.4 | **`uncertainty_available` fail-closed** | 抽掉任一真值条件后置 false 并**不写** variance 产品 | 条件不全仍写 variance ⇒ 红 |
| C5.5 | **逐像素 SNR ≠ 点源 SNR** | 两者分别输出并标注口径 | 用逐像素 SNR 冒充点源探测 SNR ⇒ 红（`13_integration.md:6` 红线） |
| C5.6 | **误差预算可复算** | 式 (5.4) 的每一项都能由 `run/reverse_verify/snr_design/exp5_error_budget.json` 的输入复算 | 任何一项无证据来源 ⇒ 红 |
| C5.7 | **相关噪声标记** | 输出 `correlation_kernel_ref` 或显式声明「对角近似，低估 20.2%」 | 静默输出对角方差 ⇒ 红 |

---

## 6 ⑤ 参考文献与借鉴点（完整逐条见 `reverse_verify/references/bibliography.md`）

### 6.1 直接进入本设计的核心引用（每条均给可核对标识）

| 用途 | 引用 | 借鉴 | 不借鉴 |
|---|---|---|---|
| 帧级 SNR 与逆方差换算 | Horne 1986, PASP 98, 609（项目冻结引用，`PSF_SIGNAL_WEIGHT.md` §9） | 最优提取 `Q = aPᵀC⁻¹d`、`W = a²PᵀC⁻¹P`、`Var(F̂) = 1/W` | 假设 PSF 与噪声已知且高斯 |
| 同上（项目冻结） | Naylor 1998, MNRAS 296, 339 | 多帧联合估计 `F̂ = ΣQ/ΣW`、`Var(F̂) = 1/ΣW` | 同上 |
| **孔径 SNR 的规范方程** | **Bertin & Arnouts 1996, A&AS 117, 393, DOI 10.1051/aas:1996164**（当前手册 **式 (36)**，已逐字核对） | `FLUXERR = sqrt(Σ(σ_i² + p_i/g_i))`——**扣天光均值、留天光方差**的权威陈述；同页自带「这是下界」的警告 | 网格背景作为最终产品（不附不确定度）；孔径形式原样用于 PSF 测光；把 `FLUXERR` 当总误差 |
| CCD 噪声预算结构 | Howell 2006, CUP, DOI 10.1017/cbo9780511807909（Ch. 4，等式号未核） | CCD 方程的项结构（源/天光/暗流/读出/平场） | 孔径框架；方程号未核不得写 |
| 校准帧方差实现 | `ccdproc.create_deviation` 文档 `[page]` | `σ = sqrt(data/gain + readnoise²)` 的形状 | 它省略平场/响应项与暗流项 |
| 三态不确定度数据模型 | Astropy `CCDData`/`NDData` 文档 `[page]`；Astropy Collab. 2022, ApJ 935, 167, DOI 10.3847/1538-4357/ac7c74 | `StdDev`/`Variance`/`InverseVariance` 三态；以逆方差为主产品 | 逐像素对角的隐含假设；对非线性运算不足 |
| 逐像素方差源自响应模型 | Bernstein et al. 2017, PASP 129, 114502, DOI 10.1088/1538-3873/aa858e | 从仪器响应模型导出方差，含**平场不确定度**项 | DECam 专用参数化；有专用定标数据的假设 |
| 方差面作为一等产品 | Waters et al. 2020, ApJS 251, 4, DOI 10.3847/1538-4365/abb82b（实页核对正文） | 噪声图**不**施加到科学图，而是构造逐像素 variance 的 weight image；科学图/权重图显式分离 | per-chip 增益归一化；「全局权重图经 warping 后仍正确」的假设 |
| 加性+乘性同时拟合 | **Burke et al. 2018, AJ 155, 41, DOI 10.3847/1538-3881/aa9f22** | **同一个全局最小二乘问题里显式分离加性与乘性**——UPM 的结构核心；前向框架 | DES 专用参数化；加性场维度远低于本项目 |
| 相对定标从重叠帧解 | Schlafly et al. 2012, ApJ 756, 158, DOI 10.1088/0004-637X/756/2/158 | 数据驱动的相对响应解（`g_k` 拟合的先例） | 静态平场假设；无加性天光场 |
| 相对/绝对解耦 | Padmanabhan et al. 2008 (ubercal), ApJ 674, 1217, DOI 10.1086/524677 | 相对/绝对解耦；同时拟合逐帧响应与光滑空间场；「隔离空间误差模式」 | **只有乘性，无逐帧加性天光场**（关键结构差异） |
| 系统项地板 | Stubbs & Tonry 2006, ApJ 646, 1436, DOI 10.1086/505138 | 方差模型必须含系统响应项；1% 测光是系统问题 | 完整仪器化定标计划的假设 |
| **稀疏重建的闭式方差** | **Rasmussen & Williams 2006, GPML, MIT Press, DOI 10.7551/mitpress/3206.001.0001** | 预测协方差 `K** − K*(K+σ_n²I)⁻¹K*ᵀ`——**正是 `PΣPᵀ` 结构**；插值权重由协方差结构决定 | `O(N³)` 稠密预测；平稳性假设 |
| 采样充分性判据 | Shannon 1949, Proc. IRE 37, 10, DOI 10.1109/JRPROC.1949.232969 | 控制点间距必须细于最小尺度的一半——**D2 的判据来源** | 带限/无噪/无限域假设（本项目全不满足，只作判据与误差界） |
| 光滑空间场（线性光滑器） | Eilers & Marx 1996, Stat. Sci. 11, 89, DOI 10.1214/ss/1038425655 | P-spline：基分辨率与光滑度解耦；**协方差闭式** | 一维表述；球面需注意坐标畸变；同方差假设 |
| 误差传播规则 | Ku 1966, J. Res. NBS 70C, 263, DOI 10.6028/jres.070c.025 | `Var(Ax) = A Var(x) Aᵀ` 的规范出处 | — |
| `C_θ` 的规范 | GSL 参考手册（Nonlinear Least-Squares Fitting → Covariance matrix of best fit parameters）`[page]`；Hogg, Bovy & Lang 2010, arXiv:1008.4686 | 参数协方差由残差 Jacobian 定义 | 需要已知协方差到差一个尺度 |
| **禁止 reduced-χ² 重标定** | **Andrae, Schulze-Hartung & Melchior 2010, arXiv:1012.3754** | 「非线性模型自由度未知」——`C_θ` 不得用 reduced-χ² 重标定 | — |
| 逆方差加权的 BLUE 性质 | Aitken 1935, Proc. R. Soc. Edinb. 55, 42, DOI 10.1017/S0370164600014346 | 逆协方差加权是 BLUE；相关误差下用 `w = C⁻¹1` | 协方差已知的假设（本项目是估计的） |
| 相关噪声下的测量 | Melchior et al. 2018 (scarlet), DOI 10.1016/j.ascom.2018.07.001 | 在 coadd 相关噪声下正确测量的公开示范 | 多波段源分离目标；紧支撑假设 |
| 点源最优叠加 | Zackay & Ofek 2017, ApJ 836, 187/188, DOI 10.3847/1538-4357/836/2/187 与 …/188 | 点源最优是 PSF 匹配滤波而非逆方差平均——**§5.2 边界的依据** | background-dominated 极限；平稳噪声谱 |
| 加性/乘性两个独立旋钮 | SWarp 文档 `[page]`；Jacob et al. 2010 (Montage), arXiv:1005.4454 | `SUBTRACT_BACK` 与加权模式分离；差分背景改正与通量守恒分离 | 二者输出权重图而非传播后的协方差 |
| 噪声优先的阈值定义 | Akhlaghi & Ichikawa 2015, ApJS 220, 1, DOI 10.1088/0067-0049/220/1/1 | SNR 相对**局部估计的噪声场**定义 | 检测目标；ambient noise 为检测纯度调优 |
| 稳健估计的理论基础 | Huber 1964, Ann. Math. Stat. 35, 73, DOI 10.1214/aoms/1177703732 | M 估计族；「稳健」的严格含义 | i.i.d. 对称位置模型（本项目空间相关） |
| MMM / 稳健天光 | Da Costa 1992, ASP Conf. Ser. 23, 90（ADS 1992ASPC...23...90D） | MMM 天光估计器的一手出处 | 无不确定度；天光局部常数假设 |
| PSF 测光规范 | Stetson 1987, PASP 99, 191, DOI 10.1086/131977 | 迭代 PSF 测光 + 稳健局部天光；邻源污染处理 | 1987 年的计算妥协；标量天光假设 |
| ePSF | Anderson & King 2000, PASP 112, 1360, DOI 10.1086/316632 | 亚像素 PSF 模型构建 | HST 特性；大量亮星假设 |
| 孔径改正 | Dolphin 2000, PASP 112, 1383, DOI 10.1086/316630 | PSF 通量 → 无穷孔径通量的孔径改正 | HST 专用 CTE/畸变 |
| Moffat 轮廓 | Moffat 1969, A&A 3, 455（ADS 1969A&A.....3..455M；页码未核） | Moffat 轮廓的参数化与出处 | 对大气湍流无物理基础；不得当作真实 PSF 描述 |
| 球面等面积控制点 | Górski et al. 2005 (HEALPix), ApJ 622, 759, DOI 10.1086/427976 | 等面积、层级、等纬度；邻居/插值结构 | CMB 谐分析机械；带限假设 |
| HEALPix 插值实现 | Zonca et al. 2019 (healpy), JOSS 4, 1298, DOI 10.21105/joss.01298 | 可引用的插值实现 | 几何插值**不**传播不确定度 |
| Gaia 合成测光 | Gaia Collab. (Montegriffo et al.) 2023, A&A 674, A33, DOI 10.1051/0004-6361/202243709 | 用户通带合成测光的定义与不确定度 | 通带一致的假设（必须声明为相对参考） |
| Gaia 测光验证 | Riello et al. 2021, A&A 649, A3, DOI 10.1051/0004-6361/202039587；Evans et al. 2018, A&A 616, A4, DOI 10.1051/0004-6361/201832756；Gaia Collab. (Drimmel et al.) 2023, A&A 674, A37, DOI 10.1051/0004-6361/202243797 | 参考星质量与定标地板 | 版本不得混用 |
| 巡天管线架构 | Bosch et al. 2018, PASJ 70, S5, DOI 10.1093/pasj/psx080；Morganson et al. 2018, PASP 130, 074501, DOI 10.1088/1538-3873/aab4ef | detrending + coadd 一体架构；方差面一等产品 | LSST-DM 数据模型；DES 的「先定标后 coadd」次序 |
| 静态天光模型 + clipped mean | LSST `CompareWarpAssembleCoaddTask` 文档 `[page]` | 「clipped mean + 静态天光模型 + 逐 warp 偏差标记」——与 UPM 结构上同一问题 | tract/patch 几何；Butler 层 |

### 6.2 明确**不借鉴**并说明理由的项（负面清单，与借鉴清单同等重要）

| 来源 | 不借鉴的东西 | 理由 |
|---|---|---|
| `reproject.reproject_and_coadd` | 「它替我们传播方差」 | **API 页已核**：只返回合并数组与 footprint，无方差/协方差输出，`combine_function` 无方差感知选项 ⇒ 传播必须自己实现 |
| 压缩感知（Candès et al. 2006, DOI 10.1109/TIT.2005.862083） | 「规则网格上的精确重建保证」 | 保证要求稀疏基不相干的**随机**采样；规则 HEALPix 网格与任何光滑基高度相干 ⇒ **不得**对规则网格声称压缩感知保证 |
| PhotometricMosaic（PixInsight） | 其估计器、其无不确定度传播、其许可条款 | 文档 URL **HTTP 404 且无存档快照**（`[UNVERIFIED]`）；本地源码明示「free for personal use only … may not redistribute or modify」⇒ **不得复制**。概念层的加性/乘性分离改用 Burke et al. 2018 与 SWarp 作引用 |
| PixInsight ImageWeighting | 其 PSFSNR 的**功率比**口径与版本相关常数 | `07_noise_snr.md:66` 明令：功率比型不能再做 `SNR²/F_ref²` 换算；常数随版本变（c3 = 1.350e-7 article / 1.316e-7 PCL 2.10.4）⇒ 不可移植 |
| SExtractor 背景 mesh | 作为**最终**天光产品 | 其插值不是统计最优重建，且**不附不确定度**；UPM 必须给天光场附方差 |
| `photutils` 的 `error=` 语义 | 「给它背景误差就够了」 | 文档已核警告：它假定 `error` 是**总**误差（含源泊松），否则会返回错误 SNR |
| ubercal | 「它已经处理了天光」 | 它只有乘性模型，**没有逐帧加性天光场** |
| 天光面模块 | 「它可以替代稀疏 SNR 层」 | `node_spacing_deg = 1.0°` 与 64 px（约 0.1″–几″ 量级）差约 2 个数量级 |

### 6.3 引用纪律

- 每条引用给**可核对标识**（DOI/arXiv/ADS bibcode/实页 URL + 抓取日期）；
- **禁止编造**：本轮未核到的（ASCL ID、`reproject` 的 JOSS DOI、Merline & Howell 1995、任何未读全文的等式号）**一律不收录**，列在 `bibliography.md §9`；
- **不得**把仓库内文档的结论当作一手证据（仓库文档只作**交叉引用**，其引用的文献已在本轮独立核对）；
- 代码引用给 `file:line`；本项目源码**只读**。

---

## 7 ⑥ 差距表（现状 / 目标 / 差距 / 修复面）

### 7.1 Phase 1

| # | 现状（含 file:line 证据） | 目标 | 差距 | 修复面 |
|---|---|---|---|---|
| P1-1 | 生产 TU `astrocs_phase1_noise`（`CMakeLists.txt:620-623`）只编 `wrapper_phase1/noise_model.cpp` + `wrapper_phase1/snr_frame_science.cpp` + `cpp/src/snr_science.cpp`；文档冻结的 `cpp/src/noise_model.cpp`（8×8 patch + 逐星掩膜 + 5σ 裁剪 + 空间场）**不在其中** | 文档冻结的掩膜 patch 噪声模型 | **文档噪声模型完全未运行** | 把 `cpp/src/noise_model.cpp` 加入 `astrocs_phase1_noise`；`add_subdirectory(lib/algorithms/noise_snr)` 当前被注释（`CMakeLists.txt:240-245`） |
| P1-2 | 帧标量 σ = **整帧未裁剪 MAD**（`wrapper_phase1/noise_model.cpp:164-190`） | σ_bg 来自**掩膜 patch** MAD | **实测 σ 偏大 1.3127×（方差 1.7676×）**，与 SNR 实际使用的 σ 矛盾 | 同 P1-1；`module_adapters.cpp:3568` 换实现 |
| P1-3 | SNR 用的 `σ_sky` = 星检测器**整帧裁剪 RMS**（`star_detector.cpp:41-67`），**无源掩膜** | 掩膜 patch σ | 源污染偏差；**同一产品内两个「噪声」互差 1.3×** | `module_adapters.cpp:3596`（`cfg.sigma_sky_adu`） |
| P1-4 | drizzle 只加 `"data"` block（`module_adapters.cpp:3962`）⇒ `has_variance=false` ⇒ `uncertainty_available=false`（实测 `n_ivar_tiles: 0`, `n_variance_tiles: 0`） | `var_p = Σ v_j w_jp²/D_p²` + HiPS `variance`/`ivar` | **逐像素权重链整体缺失** | 加 `aio_frame_add_block(frame, "variance", …)`；drizzle 读路径**已存在**（`hp_drizzle_api.cpp:1018-1047`） |
| P1-5 | gain/readnoise 仅来自可选用户 JSON（`module_adapters.cpp:3429-3448`），默认 0 ⇒ 源泊松项死 | CCD 方程 | SNR 只含天光限 | 从元数据推导（实测帧头**无** GAIN/RDNOISE 键 ⇒ 需另想办法） |
| P1-6 | `sources[].fwhm_px = 2.3548σ`（高斯矩，`star_detector.cpp:162`）被当作 Moffat4 的 FWHM 用（该轮廓 `FWHM = 1.230310σ`，`snr_science.cpp:123-212`） | 口径一致 | `σ_PSF` 高估 **1.914×** ⇒ `ΣP²` 偏小 ⇒ `σ_F` 偏大 ⇒ **SNR_F 系统性偏低** | `module_adapters.cpp:3493` 加转换或改检测器的 FWHM 定义 |
| P1-7 | `p1_snr.json` 的 `frame_snr` 实为 **5σ 深度**（自述 NOT a whole-frame scalar SNR）；HiPS 头写的是合成参考轮廓的 `snr_reference.snr_f` | 通量型 `F_ref/σ_F` | **语义漂移**；文档两处互斥（UNRESOLVED） | 先裁决（§11），再统一键名 |
| P1-8 | `local_snr[i] = SNR_F,i / median(SNR_F)`（自述 NOT a calibrated SNR） | 受控布局的稀疏控制点 SNR 层 | 现有「稀疏」是**按源检测的副产品**，非受控布局，无覆盖保证 | 新增 `sparse_snr_layer` 生产者（§3） |
| P1-9 | `sparse_snr_layer` 配置键 parser-only（`parser.cpp:320`），**无消费者** | 可选 HiPS 标准层 | 配置无效 | 实现或 fail-closed |
| P1-10 | 生产噪声路径**无饱和域处理**；声明串只在 legacy orchestrator | 显式声明降级 | 饱和核污染 σ | 加（`NOISE_SATURATION_FILTER` 已有先例） |
| P1-11 | 母版方差**未传播**（`CALIBRATION.md:236` 登记 UNRESOLVED） | `V(y_p) = {V(r)+V(b)+α²[V(d)+V(b)]+y_p²V(f)}/f²` | 系统项缺失 | 变更 claim + 实现 |
| P1-12 | drizzle 后相关噪声**只文档化不存**（`UNCERTAINTY_AND_COVARIANCE.md:15-22`） | 存 correlation kernel/scale | 方差低估 **36.3%**（σ 20.2%） | writer 加 kernel 面 |

### 7.2 Phase 2

| # | 现状（含 file:line 证据） | 目标 | 差距 | 修复面 |
|---|---|---|---|---|
| P2-1 | 生产 UPM 无 `C_θ`（`upm.cpp:233-1158` 只出 model hash） | `C_θ = (JᵀWJ)⁻¹` | **参数项不可得** | 加只读协方差出口（D6(ii)）或提升 MA 求解器 |
| P2-2 | `const bool param_cov_included = false;` **硬编码**（`module_adapters.cpp:5610`）⇒ `uncertainty_available` 恒 false（`:5611-5612`） | 三项齐全才置真 | tier-1 权重 `1/Var(corrected)` 是**死代码** | 接 `C_θ`（P2-1） |
| P2-3 | `p2_upm_ma_c_out` = `C_stat + J_out C_θ J_outᵀ` **已实现但零调用者**（`upm.cpp:2623-2643`）；`p2_upm_ma_provenance` 同样零调用者；生产从不调 `p2_upm_ma_build` | 冻结式 | 死代码 | 接线（需先裁决模型冲突） |
| P2-4 | 生产 reject 用**样本栈 MAD**（`module_adapters.cpp:5743-5751, 6006, 6033`），**无** `σ_phase1`/`upm_variance` | `σ_eff² = σ_p1² + J C_θ Jᵀ`（`12_rejection.md:22` 硬要求；缺失 fail-closed `:43`） | 合同要求未实现 | 路由到 `p2_reject_classify`（`rejection.cpp:2566`，`:2665-2666` 已有 `σ_eff` 式，但只被 v6 自检以合成输入调用） |
| P2-5 | `p2_rejection.json` **无** `σ_eff`/噪声模型 provenance | 组合式与 profile 版本 | provenance 缺失 | 加字段 |
| P2-6 | 马赛克 variance = `cov²/W`（`module_adapters.cpp:6794-6797, 6813, 7031-7038`），只含 Phase1 ivar 或帧级 SNR² 代理 | 含 drizzle 相关、共同母版、UPM 参数 | **缺协方差项** | 积分 reducer 接入 corrected variance |
| P2-7 | 成品**不写 SNR**（`aio_hips_set_frame_snr` 只被 Phase1 调用，`astro_sphere_sink.cpp:414`） | 成品 SNR 面 | 缺失 | `p2_op_write` 加键（需冻结合同） |
| P2-8 | 生产 `g_k ≡ 1`；`multiplicative_gain_applied`/`frame_gain` 被读（`:6344, 6368-6371`）但**全仓无写入者** | `g_k` 与 `b_k` 分离估计 | 乘性归一化缺失 | 提升 MA（需先裁决） |
| P2-9 | `÷g²` 只在**不可达**代码（`weight_chain.cpp:393` 的 `w *= g*g`；`variance_propagation.h:71` 的 `Var = [PΣPᵀ + param_var]/g²`） | 硬要求 | 未生效（今天恒等式） | 随 `g` 一起启用 |
| P2-10 | 控制点 64 px 均匀网格，**非自适应**；`control_reliability` 实为配置常量 1.0（SC-005） | 几何可靠性 | 承诺未实现 | 实现或订正文档 |
| P2-11 | `k_corr = 1.4` 的 MC 测试是**构建孤儿**（`control_median_mc_test` 未登记 CMake/ctest/CI） | 可复跑定标 | 证据不可复现 | 恢复该测试 |
| P2-12 | `p2_corrected.json` **确实**写了逐帧 `PΣPᵀ`（exclude-self，`:5091-5098, 5109-5175, 5642-5646`）但 `uncertainty_available` 恒 false ⇒ 该面是 **write-only** | 进权重链 | 有值无用 | 同 P2-2 |
| P2-13 | 天光面（默认启用）**不输出**系数协方差 | `J_out C_θ J_outᵀ` 的天光分量 | 式 (4.3) 参数项不完整 | 天光面模块加协方差对角出口 |

### 7.3 稀疏 SNR 层（本文新增设计）

| # | 现状 | 目标 | 差距 | 修复面 |
|---|---|---|---|---|
| X-1 | `sparse_snr_layer.schema.json` 只写 `control_points[{x,y,sparse_snr_value}]`，**未冻结数值语义与支撑尺度** | 冻结 `value_semantics` ∈ {绝对/相对/天光} 与 `support_scale_px` | 语义未冻结 ⇒ 重建算子会把点值误当点采样 | 合同（`contracts/schemas/unified/sparse_snr_layer.schema.json` 只增不改） |
| X-2 | 无重建算子实现（除 v6 的 `bilinear_regular_grid_v1`） | kriging/GP + **返回预测方差** | 无方差 ⇒ 无法进权重分母 | 新增算子 + oracle |
| X-3 | 无 `correlation_length_px` 的测量入口 | 每帧实测 ℓ | 重建误差不可预测 | 变差函数估计器（EXP-2 已有原型） |
| X-4 | 无增益判据 SP-0 的实现 | 式 (3.3) | 会无条件启用稀疏层 ⇒ 实测净亏 6.8% | 加判据门 |
| X-5 | kriging nugget 陷阱无守护 | 必须用 `sigma_value²` | 用 `1e-10` 会使预测方差恒 0（EXP-4 首轮真实发生） | 判据 C3.2 |

---

## 8 ⑥ 可验证判据汇总（判据先行，先写阈值再看结果）

全部判据已按块列在 §2.6（C1.x）、§3.8（C3.x）、§4.9（C4.x）、§5.6（C5.x）。
**判据设计的四条纪律**（`reverse_verify/README.md §3`）：

1. **已知真值**：每个合成实验给出真值（EXP-1 的解析式、EXP-4 的核）；
2. **能红能绿**：每个判据都配一个**必须变差**的负例（打乱控制点、`1e-10` nugget、朴素方差式、功率比 SNR 换算…）；
3. **判据先行**：阈值在跑之前写入脚本常量，**不得**事后放宽（EXP-1 的解析式先于 MC、EXP-3 的 SP-0 先于实测）；
4. **可复跑**：`reverse_verify/experiments/snr_design/run_all.sh` 一键复现全部 JSON。

**本轮已经红了的判据（诚实登记，不得掩盖）**：

| 判据 | 实测 | 结论 |
|---|---|---|
| C1.8 帧内 σ 一致性 | `σ_MAD/σ_clippedRMS = 1.3127`（阈值 ≤ 1.05） | **红** |
| C3.4 稀疏层与逐源 SNR 交叉验证 | Pearson(log) = 0.276（阈值 > 0.5） | **红**（口径差异，见 §3.8 待定项） |
| C3.5 增益判据 SP-0 | 帧级标量惩罚 1.0013 vs 稀疏 1.068 | **红**（该数据集上稀疏层净亏） |
| C4.6 `uncertainty_available` 真值条件 | 恒 false（`param_cov_included` 硬编码） | **红**（但这是**正确的 fail-closed**；缺陷是能力缺失） |
| C4.8 reject 阈值含参数不确定度 | 生产用样本栈 MAD | **红** |

---

## 9 诚实边界与待定项

| # | 待定内容 | 为什么不能定 | 判定方法 |
|---|---|---|---|
| U1 | **`frame_snr` 的语义**：文档两处互斥（`CONTROL_WEIGHT_SNR.md:11-14` 说「相对质量权重场，不是科学 SNR」vs `07_noise_snr.md:39` 说「未加权的原始通量型 SNR」） | 两篇权威打架，**本文无权裁决** | 上呈负责人；本文按「通量型」设计（有 `W_info` 与 `SNR = F/σ_F` 的闭式支撑），但**不**声称另一口径为错 |
| U2 | **UPM 模型**：冻结的纯加性（`PHASE2_UPM.md:47-75`）vs 目标态的乘性+加性样条（`11_upm.md:25-28`） | 两模型**不可同时为真**（`PHASE2_UPM.md:182` 已登记 UNRESOLVED） | 上呈负责人；本文的 `÷g²` 与 `J_out` 乘性分量按**目标态**写，并明确标注「当前 `g≡1` 故为恒等式」 |
| U3 | **C3.4 的口径差异**：逐源 SNR 与局部噪声 SNR 相关只有 0.276 | 未做归一化对照实验 | 把逐源 SNR 除以 `√(ΣP²)/σ_pix` 因子后再比较；若相关性显著上升则确认差异来自口径，否则两条路径之一有缺陷 |
| U4 | **C1.9 的绝对偏差量级**：`fwhm_px` 口径错配导致的 SNR_F 偏低倍数 | 未用 NumPy oracle 复算 Moffat4 的 `ΣP²` | 对同一 `(F, FWHM)` 分别取 `σ_PSF = FWHM/1.230310` 与 `FWHM/2.3548`，比较 `A_NEA = 1/ΣP²` 与 `σ_F` 的解析比值 |
| U5 | **稀疏 SNR 层在真实混合数据集上的净收益** | 本轮只有 4 帧同条件重叠数据（`t2_m2_red`），帧间权重差异太小 | 在 49 帧全集上按 `Var_p(δ_空间省略)` 分组，找出 `δ` 大的 tile 子集复测 SP-0 |
| U6 | **`k_corr = 1.4` 的可靠性** | 其 MC 测试是构建孤儿，本轮**不能**复跑 | 恢复 `control_median_mc_test` 后复跑；本文所有 `control_variance` 相关量都按 `k_corr` 线性缩放（`PHASE2_UPM.md:89`），故结论对该常数不敏感 |
| U7 | **母版方差项** | `CALIBRATION.md:236` 登记 UNRESOLVED 且无项目公式 | 按 `σ_master/σ_sky ~ 1/√N_master` 建模并标定 `N_master`；本轮只给量级占位 |
| U8 | **`reproject` / SWarp / Tractor / PSFEx 的 ASCL ID** | 本轮 `ascl.net` 不可达 | 自行核对后再引；本轮**不收录** |

**方法论边界（必须写进论文的 limitations）**：

1. **EXP-2/EXP-3 只用了一个真实天区**（M42 附近的 tile，`t2_m1_red` 与 `t2_m2_red`）与少数帧；
   所有「实测」数字都是**该数据集**的，不是普适常数。相关长度 ℓ ≈ 48 px、帧间 σ 散差 1.06 都必须在别的数据集上复测。
2. **EXP-1/EXP-4/EXP-5 是合成/解析结果**，其真值明确，但假设（等权、平稳核、独立帧）与真实数据有距离。
3. **未做端到端注入-回收**（C5.1 尚未执行）：本轮**没有**实现任何生产代码，因此无法在真实管线里注入源并回收 SNR。
   这是本设计交付后**必须补的第一件事**。
4. **未与 `photutils`/`SEP` 对拍**（C1.2）：本环境**未安装** photutils 与 sep。
5. **未复算 `k_corr`、未复跑 `control_median_mc_test`**（构建孤儿）。

---

## 10 附录：数值实验清单（可复跑）

| 实验 | 脚本 | 输出 | 内容 |
|---|---|---|---|
| EXP-1 | `experiments/snr_design/exp1_weight_penalty.py` | `exp1_weight_penalty.json` | 权重误差的效率惩罚律 `1 + Var_p(δ)`；共模不变性；残差制造者方差 `(N+1)/(N−1)`；等权 vs 最优基线 |
| EXP-2 | `experiments/snr_design/exp2_sparse_snr_reconstruction.py` | `exp2_sparse_snr_reconstruction.json` | 真实帧局部 σ/SNR 场的变差函数与相关长度；5 种间距 × 5 种重建算子的 RMSE；打乱负例 |
| EXP-3 | `experiments/snr_design/exp3_multiframe_weight_penalty.py` | `exp3_multiframe_weight_penalty.json` | 8 帧 `σ_MAD/σ_clippedRMS` 一致性；真实 4 帧的**权重效率惩罚**（本设计最关键的一张表） |
| EXP-4 | `experiments/snr_design/exp4_kriging_scaling.py` | `exp4_kriging_scaling.json` | kriging vs 双线性的误差随 `Δ/ℓ` 的解析标度律（SE 核与 OU 核）；网格不可分辨功率占比 |
| EXP-5 | `experiments/snr_design/exp5_error_budget.py` | `exp5_error_budget.json` | 叠加增益（`√N` 律）；成品 σ 的误差预算合成与主导项 |

复跑：

```bash
cd reverse_verify/experiments/snr_design
TMPDIR=/dev/shm/astrocs_snrd ./run_all.sh    # 结果落 run/reverse_verify/snr_design/
```

---

## 11 上呈负责人裁决的事项（不自行决定）

1. **U1 `frame_snr` 语义**（两篇权威互斥）——影响 HiPS 头写什么、Phase2 权重链回退路径的含义；
2. **U2 UPM 模型**（纯加性冻结 vs 乘性+加性目标态）——影响 `÷g²` 与 `J_out` 乘性分量是否进入本期实现；
3. **稀疏 SNR 层是否启用**——本文给出判据 SP-0 与实测反例（同批同条件帧上净亏 6.8%），
   建议**默认关闭、按判据启用**，而不是无条件启用；
4. **`uncertainty_available` 的期望值**——当前恒 false 是**正确**的 fail-closed；
   若要它变 true，必须先完成 P2-1/P2-2，**不得**放宽真值条件；
5. **成品是否输出 SNR 面**——`DATA_SEMANTICS.md:1108-1113` 的目标合同只冻结了 variance/ivar，
   本文建议增加 `snr/` 与 `ASTROCS_SNR_CHAIN_ID`，属**合同变更**，需负责人批准；
6. **C3.4 的口径差异**是否视为缺陷（若视为缺陷，则 `p1_snr.json` 的逐源 SNR 需要重做）。

---

## 12 交付边界声明

- 本文**只做方案设计**：`lib/`、`docs/`、`tests/`、`ci/` **零改动**（已核）；
- 全部实验代码与报告落 `reverse_verify/`；中间产物落 `run/reverse_verify/snr_design/`；
- **零 git 写权限**：本轮未做任何 git 操作；
- **未运行** `ninja`/`cmake`/`ctest`（`reverse_verify/CMakeLists.txt` 是 standalone 占位，本轮实验均为 Python，无需构建）；
- 原本误落在 `tests/validation/release02/snr_design/` 的 5 个脚本与 4 个结果 JSON **已迁移**至
  `reverse_verify/experiments/snr_design/`，原目录已删除，`tests/` 树恢复原状（已核：`tests/validation/release02/` 下无 `snr_design`）。

**可复现性信息**：

```text
仓库修订:  git rev-parse HEAD = ddf692edd3eb503b26e5579e8525043a47d8abf4
           (2026-09-19  feat(reverse_verify): 新建逆向验收工作区(根目录, 独立构建) + 负责人确认登记)
独立构建:  reverse_verify/CMakeLists.txt 是 standalone project；本轮实验全部为 Python，**未构建**，
           **未链接**主线静态库（因此无需 -DASTROCS_BUILD_DIR）；构建目录约定 run/reverse_verify/build 未被使用。
环境:      numpy 2.2.4 / scipy 1.15.3 / astropy 7.0.1；photutils 与 sep **未安装**（见 §9 边界 4）。
临时目录:  TMPDIR=/dev/shm/astrocs_snrd（已清理）。
```



