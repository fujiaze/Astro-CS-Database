# 插件文档：noise_snr（噪声 / 帧级 SNR / 点源信息量）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：从校准方差、背景、PSF 与光度响应估计逐像素噪声、逐源 SNR、深度 `m5`、点源信息量 `point_information` 与**帧级 SNR**（写入 HiPS 文件头的唯一帧级参考）。**Phase1 的 HiPS 是唯一带 SNR 数据块的产品**（帧级标量 + 可选稀疏**绝对** SNR 控制点层）。
- **不是**：不产出含义模糊的单一 `snr` 字段；不把 median source SNR 当科学叠加权重；不产出外挂独立 SNR 文件；**不替 Phase2/Phase3 产出 SNR**——Phase2 在集成时现场消费单帧 SNR 换算逆方差权重、不输出 SNR 面，Phase3 无 SNR 数据块。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §2.2（跨帧可用的绝对信噪比）、§3.1（全程只有 SNR）、§4.4（输出合同）
- `docs/design/UNIFIED_MODEL.md`（数据对象表：frame_snr、sparse_snr_layer）
- `docs/science/NOISE_MODEL.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`（噪声模型与帧级 SNR 公式正本）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §8（SNR 与 PSF 信号权重）
- `docs/research/SNR_WEIGHT_RESEARCH_PACK.md`（PixInsight 公开方法学、开源对照实现与文献研究包）

## 3. 输入/输出数据合同

- **输入**：定标信号、variance/ivar、validity、PSF 模型/地图、光度响应 `a_k`、检测目录。
- **输出**（独立对象，禁止混装）：
  - `source_snr`：`SNR_s = F_hat_s / σ_F,s`（源测量诊断，依赖源亮度）；
  - `depth_m5`：`m5 = ZP − 2.5log10[5σ_F(ref)]`（帧/位置深度表达）；
  - `point_information`：`W_psf(x,y) = a²PᵀC⁻¹P = 1/Var(F_hat)`（点源严格权重）；
  - **`frame_snr`**：帧级 SNR，**写入 HiPS 文件头**；语义 = **点源（PSF）信号 SNR**（纯信号/噪声，红线见 §4.1）；与 `sparse_snr_layer` 是**相互独立**的两个对象，**不作**稀疏层的尺度基准；
  - **`sparse_snr_layer`**（`sparse_snr_layer=true` 时）：帧内稀疏控制点 SNR 层，作为标准层插入 HiPS 文件内；控制点值 = 该点的**绝对**通量型 SNR `F_ref/σ_F(x,y)`（与帧级 SNR 同口径、同逐帧参考通量 `F_ref`，无量纲），消费时由控制点**直接重建**为稠密 SNR 场；**默认产出**（默认稀疏路径，§4.2）。
  - **不输出**：Phase2/Phase3 产物不含 SNR 面/块；Phase2 只在集成中现场消费本模块产出的单帧 SNR 换算逆方差权重，不复用本模块的 SNR 产物。
- 参考：`eng/contracts/schemas/noise_snr_output.schema.json`。

## 4. 算法与公式要点

```text
W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k = 1/Var(F_hat_k)
白噪声: W_psf,k = a_k² Σ_p P_k,p² / σ_pix,k² = a_k² / (σ_pix,k² A_NEA,k)
SNR_k²(F_ref,k) = F_ref,k² W_psf,k
```

### 4.1 帧级 SNR（frame_snr）

- 是**唯一帧级参考**，写入 HiPS 文件头；
- **是未加权的原始信噪比，不是权重**——它只描述「这一帧的真实源信号相对真实噪声有多强」这一客观测量事实，不包含任何为某次叠加服务的加权；权重由 Phase2 逆方差集成时从 SNR 现场计算（最高设计 §5.3）；
- **信号经独立的局部背景估计与扣除**：天光不进入信号项；把天光计入信号的普通 SNR 会随天光变亮而虚高，不能作唯一帧级参考；
- **天光只通过散粒噪声进入噪声项**：天光变亮 ⇒ `σ_F` 增大 ⇒ SNR 单调下降，`B→∞` 时 `SNR→0`；这是真实物理变化，不是指标漂移。

**帧级 SNR 红线（不可协商）**：

- `SNR = F_signal / σ_F`，`F_signal` **必须已扣局部背景**；天光**只作为噪声项**进入 `σ_F`；
- 固定源通量、增大天光 ⇒ SNR **单调下降**（`B→∞` 时 `SNR→0`），该性质必须可复现；
- 帧级 SNR 是**点源（PSF）**量，**不得**与面亮度 SNR 混用或互相宣称等价；
- 定义式（`F_signal` 的测光口径与 `σ_F` 的稳健噪声估计）以 `docs/science/PSF_SIGNAL_WEIGHT.md` 与 `docs/science/NOISE_MODEL.md` 为正本；
- **验收方式**：对每条 SNR 路径做**注入-回收**——已知真值信号 + 已知天光 + 已知噪声，回收的 SNR 必须等于真值 `F_s/σ_F`；不满足者否决或修正。

**与 PixInsight 公开方法学的关系**：PixInsight 核心闭源，但其《New Image Weighting Algorithms》参考文档公开了完整方法学，其中是**两个不同的量**，必须区分：

| 量 | 定义 | 性质 | AstroCS 对应 |
|---|---|---|---|
| **PSFSNR** | ratio-of-powers 信噪比（官方文档式[18]）：`c3·(Σ_j f_j)² / (c4·σ_n²)`，分子是**和的平方**，f_j 为各星 PSF 通量（FWTM 孔径内像素减局部背景求和），σ_n 为稳健噪声（MRS/N*）；c3、c4 是官方模拟数据标定常数 | **未加权的原始信噪比**（功率比口径） | **frame_snr 对标此量的方法学**（信号取数、稳健噪声、独立背景三点），数学定义采用通量型口径以保证逆方差换算严格成立，不逐字套用此式，独立标定常数、不照抄 c3/c4 |
| **PSF Signal Weight** | 综合图像质量权重：信号总量 × 信号集中度（mean flux，随 FWHM 变小而增大）/（稳健噪声 × 稳健平均背景） | **权重**，额外含分辨率/FWHM 与背景梯度惩罚，不是信噪比 | 不采用；PSF 拟合质量代理（FWHM、残差尺度等）只作诊断，**不计入科学叠加权重** |
| 标准 SNR | `σ²/σ_n²`，全局尺度估计 | 信噪比，但**受天光/梯度正向影响**，会给亮背景帧虚高权重 | 不采用 |

- 共同借鉴的方法学：① 信号只从检测到的恒星经 PSF/孔径混合测光得到（不用拟合振幅，只用采样像素减独立估计的局部背景）；② 噪声用稳健多尺度估计；③ 背景（天光）作为**独立的稳健分量**估计与扣除，不进入信号——这三点保证 frame_snr 的信号项不被天光背景虚高（天光散粒噪声仍计入 `σ_n`）；
- PixInsight 官方同样**不把权重存进图像**：校准阶段只把信号/噪声/背景分量写入元数据（FITS 关键字 PSFFLX/PSFMFL/PSFMST/PSFNST/NOISE 等），权重在 ImageIntegration 集成时才计算——与 AstroCS「数据库存原始 SNR、Phase2 消费时才算权重」的设计一致；
- PSFSW 归一化常数与 PSFSNR 常数（文章版 c3=1.350×10⁻⁷、c4=4.987×10⁺⁶；PCL 2.10.4 源码 c3=1.316×10⁻⁷，存在版本漂移，引用须带版本）均由 PixInsight 自造模拟图标定，**AstroCS 不照抄**：采用无量纲/物理量纲定义，常数由本项目合成数据
  标定，不引用其经验常数。（验收 L1）独立标定并冻结。

**数学定义与换算**（**逐帧**参考通量 `F_ref,k` + 公共锚 `F0`，正本见 `docs/science/PSF_SIGNAL_WEIGHT.md`）：
```text
# 帧级 SNR（未加权原始信噪比，写入文件头）：真实源信号 / 真实噪声（通量型口径，Horne 1986）
SNR_k(F_ref,k) = F_ref,k · sqrt(W_psf,k) = F_ref,k / σ_F,k
W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k  （点源信息，σ_F,k² = 1/W_psf,k；信噪比本身未做任何加权）
F_ref,k = 10^(−0.4·(m_ref − ZP_k))，m_ref = 6.0；F_ref,k · k_photo,k = F0（公共锚，严格恒等）

# Phase2 集成时现场换算为逆方差权重（UPM 已归一到公共通量尺度）：
w_k = 1/σ_F,k² = SNR_k(F_ref,k)² / F_ref,k²   ⇒  w_k ∝ SNR_k²（配对性只在同一帧内成立）
```

- **逐帧 `F_ref,k`（不是组内公共常数）**：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))` 由**该帧自己的** `ZP_k` 决定，`m_ref = 6.0`，`reference_flux_scope = frame_independent_fixed_magnitude`；公共锚 `F0 = 10^(−0.4·(m_ref − ZP_syn))` 与帧无关，严格恒等 `F_ref,k · k_photo,k = F0`（实测偏差 ≤ 4.3e-4）。配对性只要求**同一帧内** SNR 与 `F_ref` 同源，**不要求跨帧相等**；不同指向/不同光学系统的帧**合法地**有不同 `F_ref,k`，**不设组间 `F_ref` 硬闸门**。
- **`m_ref = 6.0` 适用域警告**：`ZP_syn` 由 Gaia DR3 XP 绝对 XPSD 谱经本帧滤光片/QE **正向合成**，`F_ref,k` 是在**线性区外**的形式外推（实测：M42 Red 300 s 超饱和 1899×；HST M16 F657N 超 WFC3/UVIS 满井 1.9e4×）。作为**参考电平仍良定义**（天光限下 `SNR ∝ F_ref`），但**不得**表述为「本帧能测到的 6 等星」。
- **口径澄清（与 PixInsight 式[18]的区别）**：上式 frame_snr 是**通量型**信噪比 `F_ref,k/σ_F,k`（一次方比），逆方差换算 `w_k=SNR_k²/F_ref,k²=1/σ_F,k²` 严格成立；PixInsight 式[18] PSFSNR 是**功率比型** `(Σf)²/σ_n²`（平方比，本身已是 SNR² 量级），不能再做 `SNR_k²/F_ref,k²` 换算。AstroCS 只对标 PSFSNR 的方法学（恒星测光取信号、稳健噪声、独立背景），数学上采用通量型口径以保证与逆方差叠加严格自洽。
- HiPS 是数据库：帧产品长期保存、可被任意多次、任意科学目标的叠加消费，因此入库的是客观的未加权 SNR（与具体集成无关的观测量）；权重在 Phase2 集成时按天球像素对应的输入帧集合现场计算。
- 稀疏帧内层启用时，每个控制点同样存**未加权的绝对 SNR(x,y)**（与帧级 SNR 同一物理定义、同一逐帧参考通量 `F_ref`；绝对量本身，不是相对因子），而非权重。

### 4.2 SNR 三条路径与稀疏帧内层

**三条路径全部保留在算法面**（Phase1 产出 / Phase2 重建），由配置 JSON 显式指定：

| 路径 | Phase1 产出 | Phase2 重建稠密 | 定位 |
|---|---|---|---|
| `dense` | 稠密逐像素 SNR 面 | 直接使用 | 精度基准 |
| `sparse_reconstruct`（**默认**） | 稀疏控制点**绝对** SNR 层 | 由稀疏层重建 | 存储/精度折中（现行默认） |
| `frame_reconstruct` | 仅帧级标量 | 由帧级重建 | 单帧级对照 |

- **默认 = `sparse_reconstruct`**：`sparse_snr_layer=true` 时生成稀疏控制点 SNR 层，作为**标准层插入 HiPS 文件内**；
- 三条口径都**直接**产出同一物理量 `SNR = F_ref/σ_F` 的稠密表示，只在重建方式上不同：`dense` 用 Phase1 稠密面、`sparse_reconstruct` 由稀疏**绝对** SNR 控制点重建、`frame_reconstruct` 用帧级标量；`sparse_reconstruct` **不乘帧级标量**（控制点值即绝对信噪比本身）。SNR 是信噪比，不是权重；
- 稀疏层的位置/值/采样覆盖写入 manifest；
- **适用域由实验判定**：同条件比较三条路径重建稠密 SNR 的精度与存储量，不预设稀疏一定最好；完整适用域图谱由 `实验/absolute-snr` 给出（最高设计 §5.3）；
- **不静默降级**：输入无稀疏层而路径为 `sparse_reconstruct`（含默认）⇒ 按帧级执行但**必须显式记录实际路径**（`snr_path_effective`）并计数；稀疏层存在但损坏/不可重建 ⇒ 显式失败；
- 存储量/精度折中与显式指定口径见 §5 配置项。

### 4.2a σ_sky 口径冻结（防读出噪声双计；SCI-B D1 定案）

- **唯一口径规则**：逐像素噪声 `σ_i² = σ_sky,i² + (RN/g)² + F·P_i/g`，**读噪只出现一次**。`sigma_sky_adu` 这个入参**只承载一种语义**，调用方必须显式声明是哪种，禁止静默择一。
- **入参语义枚举（二选一，必填）**：

| `sigma_sky_source` | 含义 | 是否再加 `(RN/g)²` |
|---|---|---|
| `shot_noise_only` | 天光 + 暗流的**散粒**方差（不含读噪） | **加**（默认路径，需 `gain_e_per_adu>0 && read_noise_e>0`） |
| `empirical_total_rms` | 经验**总** rms（含读出噪声，如生产调用点传入的 `noise_sigma` = `StarDetector::estimate_background` 的**整帧 2 轮裁剪 RMS**；噪声模型 A 的 `1.4826×MAD` 稳健尺度是另一生产者，承载逐像素 `variance`） | **不加**（已含读噪） |

- **决策树（调用点实现口径）**：
  1. `sigma_sky_source=shot_noise_only` 且 `gain_e_per_adu>0 && read_noise_e>0` ⇒ 走 `σ_sky,散粒² + (RN/g)² + F·P_i/g`（设计本意路径）；
  2. `sigma_sky_source=empirical_total_rms` ⇒ 只加源泊松项 `F·P_i/g`，**不再加** `(RN/g)²`；
  3. `gain_e_per_adu<=0`（gain 未知，PSF 行路径）⇒ 只加源泊松项，不加 `(RN/g)²`；
  4. 声明与实际来源不一致（声称散粒而来源为经验总 rms，或反之）⇒ **fail-closed 拒绝**，不得静默择一、不得只告警。
- **实测证据**（`实验/absolute-snr/results/b2_noise_terms.json`，N_MC=1000，复现 `python3 实验/absolute-snr/code/b2_noise_terms.py`）：双计使 `σ_F` 高估 **+12.8%**（基准点 F=1000 e⁻、B=100 e⁻/px、RN=10 e⁻、g=1.3、D=0.5）至 **+34.0%**（RN=50 e⁻ 最坏点）；天光主导点（B≥10⁵）偏差 <1%；正确口径臂在全部扫描点 ≤3σ。
- **帧级 SNR 实证**（`实验/absolute-snr/results/b1_sky_scan.json`）：B=0 时双计臂 SNR=38.47 vs 定义式/真值 43.32/43.21（**−11.2%**）。
- **必须保持的不变量**：固定源通量、天光 B 增大 ⇒ SNR 单调下降、斜率 −1/2；双计会破坏该斜率。
- **负例保护**：旧组合方式（经验总 rms 再加 `(RN/g)²`）必须被测试判红。

### 4.3 其他要点

- 白噪声时为简单形式；相关噪声时用完整信息核；
- 标量降级门：仅当帧内 `W_psf(x,y)` 鲁棒相对离散与系统趋势低于阈值才存帧级标量；否则存 map/控制点/多项式/HEALPix，摘要带 p05/p50/p95、最大系统偏差、覆盖、模型误差；
- PSF 拟合质量代理（FWHM、残差尺度等）只作诊断，**不计入科学叠加权重**；frame_snr 对标的是 PSFSNR（未加权原始信噪比）。

### 4.4 噪声模型：A 为唯一生产模型

- **噪声模型 A 为唯一生产模型**：实现 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp`，接口 `snr_noise_model_v1` / `_f64` / `_fill` + `NoiseWeightModelV1`（**逐像素**），编入根 `CMakeLists.txt:644-659` 的 `astrocs_phase1_noise`（STATIC），主程序链接 `:747`。
- 模型定义、参数语义、稳健噪声估计与掩膜规则的正本见 `docs/science/NOISE_MODEL.md`；本文件只登记插件侧接口与接线事实，不复制公式。
- **逐像素方差接线现状（如实登记）**：A 已编入 `astrocs_phase1_noise` 并链入主程序，但生产调度路径（`module_adapters.cpp`）尚未挂 `variance` 块 ⇒ `hp_drizzle_api.cpp:1024` 读不到 ⇒ `astro_sphere_sink.cpp:306–327` 不产出 variance/ivar 子产品面 ⇒ `p2_integrated.json` 报 `uncertainty_available=false`（原因 `ivar_product_missing_frame_snr_fallback`）。
- **接入义务**：把 `orchestrator` 的逐像素方差接线（`orchestrator.cpp:4694-4839`）搬进 `scheduler`，使生产产出逐像素 `variance`/`ivar` 产品面；接入并验证后删除 `lib/infrastructure/pipeline/orchestrator/cpp/` 与 v6 家族（最高设计 §8.2）。
- **硬化项**：`variance_floor` clamp 可产出「rc=0 + deg=0 + var=1e-12」的**伪有效模型**（仅输入误读场景复现）⇒ 须补**退化判据与守卫**（fail-closed）。
- **影响面（诚实）**：**不影响**帧级 SNR 路径（`snr_chain_closure="closed"`，科学上正确）；**影响**逐像素**不确定度产品面**。凡「逐像素方差/不确定度已传播到产品」的主张，**只能引用 A 的插件路径证据**，**不得声称生产路径已产出**。
- **行号说明**：本节的 `文件:行` 以符号名/文件名核对为准；工作树并发改动会使行号漂移。

### 4.5 稀疏帧内层几何

- 稀疏控制点间隔 Δ 复用 Phase2 UPM 的 8×8/tile 控制网格，`Δ = hips.tile_width / 8`（`hips.tile_width = 512` ⇒ Δ = 64 px）；
- Δ 与 SNR 场相关长度的关系、稀疏重建的失效边界与完整适用域，正本见 `docs/science/` 与 `实验/absolute-snr`；本文件不复制数值；
- 稀疏控制点存**绝对** SNR（与帧级 SNR 同口径、同逐帧参考通量 `F_ref`）；重建算子在控制点上重建出稠密绝对 SNR 场，并返回预测方差；帧级标量与稀疏层相互独立，不作其尺度基准，消费时也不参与还原。
- **重建算子选型 = 规范缺失（登记项）**：现行文档未冻结具体重建算法；现行实现仅提供规则网格双线性与最近控制点两种，选型结论由重建算法对比实验给出后落地。任何算子都必须满足四条**与选型无关**的约束：① **显式声明**——算子标识入 manifest；② **正齐次性** `R[a·v] = a·R[v]`（`a > 0`）——控制点值整体缩放时重建场按同一因子缩放，这是「帧内共模因子在权重口径下相消」成立的前提，带**数据无关固定先验均值**或向固定值收缩正则的算子（如固定先验均值的 GP/kriging）**不满足**该条；③ 在控制点处**精确复现**节点值；④ 越出定义域即 **fail-closed**，不得静默外推或回退帧级。不正齐次的算子若被选用，必须把 `homogeneity` 声明与「该算子下相对场**不能**由绝对场缩放得到」写入 manifest，消费侧**不得**据共模相消做任何等价假设（算子正齐次性的反例与残差量级见 `实验/absolute-snr` EXP-05 §3.1/§3.2）。
- 控制点位置、取值、采样覆盖与 `snr_path_effective` 一并写入 manifest 与产品内容证据块。
- **控制点局部 σ 必须结构感知（数值准确的必要条件）**：控制点存绝对 SNR 只保证**表示正确**，不保证**数值准确**——后者完全由局部 σ 估计器决定。把估计作用域从整帧朴素地换成区域（同一 recipe + 更小窗口）**不够**：整帧口径的结构污染只是被挪到更小尺度，cell 内的未分辨结构仍被算进稳健尺度。控制点的局部 σ **必须**用**结构感知**估计器：mesh 局部背景扣除后的逐区域残差稳健尺度，或跨帧差分（唯一零结构偏差口径）；估计器标识、`sigma_rho` 与 `quality_flags` 一并入 manifest。估计器认证、逐区域偏差与适用域见 `docs/science/CONTROL_WEIGHT_SNR.md` §8b 与 `实验/absolute-snr`（EXP-03 的 R0/R1/R2）。

### 4.6 输出独立性与禁止混装

- `source_snr` / `depth_m5` / `point_information` / `frame_snr` / `sparse_snr_layer` 各是各的对象，各有独立 schema，禁止互相顶替；
- `point_information` 是点源严格权重（`1/Var(F_hat)`），与帧级 SNR 是不同对象：前者是权重、后者是信噪比；
- 权重不落盘：HiPS 里只存帧级 SNR 与稀疏**绝对** SNR，权重由 Phase2 集成时现场派生（最高设计 §3.1）；
- 端口只接同一种对象：Phase2 集成端口只接受 `frame_snr` / `sparse_snr_layer` 与 `point_information`，接错判红（最高设计 §3.1）；
- Phase1 与 Phase3 不产生、不消费权重；全程只有 SNR。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `reference_flux` | 逐帧 `F_ref,k` | 见说明 | 参考通量（m5/SNR 定义必需）。**逐帧**：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，`m_ref = 6.0`，`reference_flux_scope = frame_independent_fixed_magnitude`；公共锚 `F0` 满足 `F_ref,k · k_photo,k = F0`。单位以产物字段 `reference_flux_common_unit` 为准（现行 = `F_syn (Gaia XPSD absolute spectral integral)`；显式 `snr.reference_flux_adu` 覆盖时为 ADU）。⚠ `m_ref = 6.0` 是**参考电平的形式外推**，**不得**表述为「本帧能测到的 6 等星」 |
| `scalar_gate_rd` | —— | —— | 标量降级鲁棒离散门 |
| `scalar_gate_trend` | —— | —— | 标量降级系统趋势门 |
| `psfsw_enable` | true | —— | 是否产出 PSF 信号权重复合分量（诊断/基线对照；PSF 拟合质量代理不计入科学叠加权重） |
| `sparse_snr_layer` | true | —— | 是否产出稀疏帧内 SNR 层（控制点值 = 绝对通量型 SNR `F_ref/σ_F(x,y)`，与帧级同口径、同 `F_ref`）。**默认产出**（默认稀疏路径） |
| `sparse_snr_spacing_px` | 64 | px | 稀疏层控制点间隔 Δ（px）：复用 Phase2 UPM 的 8×8/tile 控制网格 ⇒ `Δ = hips.tile_width / 8 = 512 / 8 = 64`。取值依据与适用域见 §4.5 |
| `sparse_snr_density` | —— | 点/度² | 稀疏层控制点密度（按面积表述）；生产使用像素域控制点间隔 `sparse_snr_spacing_px` 承载该量，本键不承载生产取值 |
| `snr_path` | `sparse_reconstruct` | —— | SNR 重建路径：`dense` / `sparse_reconstruct`（默认）/ `frame_reconstruct`；三条路径的适用域由 `实验/absolute-snr` 给出 |

## 6. 接口/ABI

- entrypoint：信号+ivar+PSF+`a_k` → {source_snr, depth_m5, point_information, frame_snr[, sparse_snr_layer]}；
- 帧级 SNR 经 drizzle 写入 HiPS 文件头；稀疏层作为标准层插入 HiPS；
- **Phase2/Phase3 不产出 SNR**：Phase2 在集成中现场消费本模块的单帧 SNR（换算逆方差权重），不输出 SNR 面；Phase3 无 SNR；
- 各类输出独立 schema，禁止混装。

## 7. 错误与边界

- 缺 `a_k`/PSF/方差 → fail-closed（信息权重不可凭空造）；
- 标量门失败 → 自动升级为空间模型（不得静默用标量）；
- `reference_flux` 未定义时 m5/SNR 不可输出；无 `photscale_fit` / `zero_point_valid=false` ⇒ 按显式回退 `group_median`（`reference_flux_scope` 落盘，**不伪造**），显式 `snr.reference_flux_adu` 优先；**逐帧中位数回退为 fail-closed，不得恢复**；
- **禁止**把组内 `F_ref` 相等当作门；缺 `variance` 块 ⇒ `uncertainty_available=false`，**不得**声称已产出逐像素方差（§4.4）；
- 帧级 SNR 无法计算（如缺真实信号参考）→ fail-closed，**不得用受天光影响的普通 SNR 代替**；
- 指定 `sparse_reconstruct` 路径而输入无稀疏层 → 按帧级执行并**显式记录实际路径**（不静默）；稀疏层损坏/不可重建 → fail-closed；
- **稀疏层值语义判红**：把控制点值按**相对因子**解释（含乘/除帧级标量做还原）⇒ 必须判红。控制点值是**绝对**通量型 SNR，由 schema 的 `sparse_snr_semantics` 冻结为 `absolute_flux_type_snr`；声明为相对语义或缺失该键 ⇒ schema 判红；
- 任何信号项未扣局部背景、被天光/背景抬高的 SNR → 判红（红线 §4.1）；
- **σ_sky 双计判红（§4.2a）**：`sigma_sky_source=empirical_total_rms` 时再加 `(RN/g)²` ⇒ 必须判红（保护负例）；
- **声明义务**：**生产调用点**（`module_adapters` 的 `p1_op_noise`）必须显式声明 `sigma_sky_source`，缺失即评审判红；声明与实际来源不一致（声称散粒而来源为经验总 rms，或反之）⇒ fail-closed 拒绝。
- **C ABI legacy 路径**：`SNR_SIGMA_SKY_UNSPECIFIED=0` 保留为直接 C API 调用方的兼容缺省（与 `SHOT_ONLY` 组合**逐位一致**，由 `p1snr_science_skysource` 的向后兼容锁固定）；该路径**禁止**在生产链使用——「缺失即 fail-closed」由调用点层（而非 C 函数层）保证，因为 C 函数层无法观测入参的**实际来源**。

## 8. 测试与 Oracle

- 注入点源：理论 `σ_F=1/√W_psf` 与实测散度一致；
- 改变星表亮度分布不改变 `W_psf`、但改变 median source SNR（跨模块验证）；
- seeing/背景/透明度按理论改变信息权重；
- **加性背景平移不改变信号项**（注入恒定背景偏置，测光信号不变）；**天光散粒噪声增强时 `σ_n` 增大、帧级 SNR 按理论下降**；**单调性负例**：固定源通量、天光 `B` 增大 ⇒ SNR 单调下降，`B→∞` 时 `SNR→0`；
- **注入-回收**：已知真值 `F_s` + 已知天光 + 已知噪声 ⇒ 回收 SNR = `F_s/σ_F`（三条路径各一组；不满足者判红）；
- 稀疏层：启用/不启用输出结构正确，稀疏层值可重建验证；**三路径精度与存储量对比**：`dense` / `sparse_reconstruct` / `frame_reconstruct` 同输入重建稠密 SNR，报告精度差与存储量；无稀疏层而路径为 `sparse_reconstruct` 时实际路径须被显式记录（负例：静默降级判红）；
- **稀疏层绝对语义判据（能红能绿）**：① 绿——控制点自身复现：在节点坐标处重建值 == 落盘控制点值（残差 ~0），且与帧级标量**无关**（同层配不同帧级标量，重建结果逐位相同）；② 红——按相对值解释：取 `SNR(x,y) = frame_snr × v(x,y)/median(v)` 时，节点重建值 ≠ 落盘值（除非 `frame_snr == median(v)`），该差异必须被判红；③ 红——schema 层：`sparse_snr_semantics` 声明为相对语义或缺失 ⇒ 合同测试判红；
- 点源/面亮度口径分离：把面亮度 SNR 当帧级 SNR 使用必须判红；
- **逐帧 `F_ref,k` 独立性负例**：人为要求组内 `F_ref` 相等（组间硬闸门）⇒ 必须判红——不同指向/不同光学系统的帧合法地有不同 `F_ref,k`；`w_k = SNR_k²/F_ref,k²` 的配对性**只在同一帧内**成立；
- 1 worker vs N worker 一致。
