# 插件文档：noise_snr（噪声 / 帧级 SNR / 点源信息量）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：从校准方差、背景、PSF 与光度响应估计逐像素噪声、逐源 SNR、深度 `m5`、点源信息量 `point_information` 与**帧级 SNR**（写入 HiPS 文件头的唯一帧级参考）。**Phase1 的 HiPS 是唯一带 SNR 数据块的产品**（帧级标量 + 可选稀疏**绝对** SNR 控制点层）。
- **不是**：不产出含义模糊的单一 `snr` 字段；不把 median source SNR 当科学叠加权重；不产出外挂独立 SNR 文件；**不替 Phase2/Phase3 产出 SNR**——Phase2 在集成时现场消费单帧 SNR 换算逆方差权重、不输出 SNR 面，Phase3 无 SNR 数据块。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §2.2（跨帧可用的绝对信噪比）、§3.1（全程只有 SNR）、§4.4（输出合同）
- `docs/detail/UNIFIED_MODEL.md`（数据对象表：frame_snr、sparse_snr_layer）
- `docs/science/NOISE_MODEL.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`（噪声模型与帧级 SNR 公式正本）
- `docs/detail/PHASE1_DETAILED_DESIGN.md` §8（SNR 与 PSF 信号权重）
- `实验/absolute-snr/docs/SNR_WEIGHT_RESEARCH_PACK.md`（PixInsight 公开方法学、开源对照实现与文献研究包）

## 3. 输入/输出数据合同

- **输入**：定标信号、variance/ivar、validity、光度响应 `a_k`、检测目录（含逐源 `fwhm`；交付样本按测光有效源构造，不取 PSF 参数面）。
- **输出**（独立对象，各自独立成面）：
  - `source_snr`：`SNR_s = F_hat_s / σ_F,s`（源测量诊断，依赖源亮度）；
  - `depth_m5`：`m5 = ZP − 2.5log10[5σ_F(ref)]`（帧/位置深度表达）；
  - `point_information`：`W_psf(x,y) = a²PᵀC⁻¹P = 1/Var(F_hat)`（点源严格权重）；
  - **`frame_snr`**：帧级 SNR，**写入 HiPS 文件头**；语义 = **点源（PSF）信号 SNR**（纯信号/噪声，红线见 §4.1）；与 `sparse_snr_layer` 是**相互独立**的两个对象，**不作**稀疏层的尺度基准；
  - **`sparse_snr_layer`**（`sparse_snr_layer=true` 时）：帧内稀疏控制点 SNR 层，作为标准层插入 HiPS 文件内；控制点值 = 该点的**绝对**通量型 SNR `F_ref/σ_F(x,y)`（与帧级 SNR 同口径、同逐帧参考通量 `F_ref`，无量纲），消费时由控制点**直接重建**为稠密 SNR 场；**默认产出**（默认稀疏路径，§4.2）。
  - **不输出**：Phase2/Phase3 产物不含 SNR 面/块；Phase2 只在集成中现场消费本模块产出的单帧 SNR 换算逆方差权重，不复用本模块的 SNR 产物。
- 参考：`docs/science/DATA_SEMANTICS.md` §13（DATA-P1-NOISE：模块输入/输出数据合同正本）。

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
- 帧级 SNR 是**点源（PSF）**量，与面亮度 SNR **各自独立、互不代用**；
- 定义式（`F_signal` 的测光口径与 `σ_F` 的稳健噪声估计）以 `docs/science/PSF_SIGNAL_WEIGHT.md` 与 `docs/science/NOISE_MODEL.md` 为正本；
- **验收方式**：对每条 SNR 路径做**注入-回收**——已知真值信号 + 已知天光 + 已知噪声，回收的 SNR 必须等于真值 `F_s/σ_F`；不满足者修正或退回重检。

**与 PixInsight 公开方法学的关系**：PixInsight 核心闭源，但其《New Image Weighting Algorithms》参考文档公开了完整方法学，其中是**两个不同的量**，必须区分：

| 量 | 定义 | 性质 | Astro Celestial Sphere Database（ACSD） 对应 |
|---|---|---|---|
| **PSFSNR** | ratio-of-powers 信噪比（官方文档式[18]）：`c3·(Σ_j f_j)² / (c4·σ_n²)`，分子是**和的平方**，f_j 为各星 PSF 通量（FWTM 孔径内像素减局部背景求和），σ_n 为稳健噪声（MRS/N*）；c3、c4 是官方模拟数据标定常数 | **未加权的原始信噪比**（功率比口径） | **frame_snr 对标此量的方法学**（信号取数、稳健噪声、独立背景三点），数学定义采用通量型口径以保证逆方差换算严格成立，不逐字套用此式，独立标定常数、不照抄 c3/c4 |
| **PSF Signal Weight** | 综合图像质量权重：信号总量 × 信号集中度（mean flux，随 FWHM 变小而增大）/（稳健噪声 × 稳健平均背景） | **权重**，额外含分辨率/FWHM 与背景梯度惩罚，不是信噪比 | 不采用；PSF 拟合质量代理（FWHM、残差尺度等）只作诊断，**不计入科学叠加权重** |
| 标准 SNR | `σ²/σ_n²`，全局尺度估计 | 信噪比，但**受天光/梯度正向影响**，会给亮背景帧虚高权重 | 不采用 |

- 共同借鉴的方法学：① 信号只从检测到的恒星经 PSF/孔径混合测光得到（不用拟合振幅，只用采样像素减独立估计的局部背景）；② 噪声用稳健多尺度估计；③ 背景（天光）作为**独立的稳健分量**估计与扣除，不进入信号——这三点保证 frame_snr 的信号项不被天光背景虚高（天光散粒噪声仍计入 `σ_n`）；
- PixInsight 官方同样**不把权重存进图像**：校准阶段只把信号/噪声/背景分量写入元数据（FITS 关键字 PSFFLX/PSFMFL/PSFMST/PSFNST/NOISE 等），权重在 ImageIntegration 集成时才计算——与 ACSD「数据库存原始 SNR、Phase2 消费时才算权重」的设计一致；
- PSFSW 归一化常数与 PSFSNR 常数（文章版 c3=1.350×10⁻⁷、c4=4.987×10⁺⁶；PCL 2.10.4 源码 c3=1.316×10⁻⁷，存在版本漂移，引用须带版本）均由 PixInsight 自造模拟图标定，**ACSD 不照抄**：采用无量纲/物理量纲定义，常数由本项目合成数据
  标定，不引用其经验常数。（验收 L1）独立标定并冻结。

**数学定义与换算**（**逐帧**参考通量 `F_ref,k` + 公共锚 `F0`，正本见 `docs/science/PSF_SIGNAL_WEIGHT.md`）：
```text
# 帧级 SNR（未加权原始信噪比，写入文件头）：真实源信号 / 真实噪声（通量型口径；Horne, K. 1986, PASP 98, 609, DOI: 10.1086/131801）
SNR_k(F_ref,k) = F_ref,k · sqrt(W_psf,k) = F_ref,k / σ_F,k
W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k  （点源信息，σ_F,k² = 1/W_psf,k；信噪比本身未做任何加权）
F_ref,k = 10^(−0.4·(m_ref − ZP_k))，m_ref = 6.0；F_ref,k · k_photo,k = F0（公共锚，严格恒等）

# Phase2 集成时现场换算为逆方差权重（UPM 已归一到公共通量尺度）：
w_k = 1/σ_F,k² = SNR_k(F_ref,k)² / F_ref,k²   ⇒  w_k ∝ SNR_k²（配对性只在同一帧内成立）
```

- **逐帧 `F_ref,k`（不是组内公共常数）**：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))` 由**该帧自己的** `ZP_k` 决定，`m_ref = 6.0`，`reference_flux_scope = frame_independent_fixed_magnitude`；公共锚 `F0 = 10^(−0.4·(m_ref − ZP_syn))` 与帧无关，严格恒等 `F_ref,k · k_photo,k = F0`。配对性只要求**同一帧内** SNR 与 `F_ref` 同源，**不要求跨帧相等**；不同指向/不同光学系统的帧**合法地**有不同 `F_ref,k`，**不设组间 `F_ref` 硬闸门**。
- **`m_ref = 6.0` 适用域警告**：`ZP_syn` 由 Gaia DR3 XP 绝对 XPSD 谱经本帧滤光片/QE **正向合成**，`F_ref,k` 是在**线性区外**的形式外推（真实帧可饱和或超满井）。作为**参考电平仍良定义**（天光限下 `SNR ∝ F_ref`），但**该值的用途 = 参考电平**（6 等星档参考）。
- **口径澄清（与 PixInsight 式[18]的区别）**：上式 frame_snr 是**通量型**信噪比 `F_ref,k/σ_F,k`（一次方比），逆方差换算 `w_k=SNR_k²/F_ref,k²=1/σ_F,k²` 严格成立；PixInsight 式[18] PSFSNR 是**功率比型** `(Σf)²/σ_n²`（平方比，本身已是 SNR² 量级），不能再做 `SNR_k²/F_ref,k²` 换算。ACSD 只对标 PSFSNR 的方法学（恒星测光取信号、稳健噪声、独立背景），数学上采用通量型口径以保证与逆方差叠加严格自洽。
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

**适用域边界（三条，判据 E = `Var_w/Var_opt − 1`，`E=0 ⇔ σ̂ ∝ σ_true`；完整适用域图谱与读数 = `实验/absolute-snr`）**：

1. **HST 类高对比域：失效边界 Δ\* 与重建算子强绑定**。Δ\* = 「该算子 E 首次劣于帧级臂的最小 Δ」：`bilinear_regular_grid_v1` 与默认档 `natural_bicubic_spline_clip_v1` 的 Δ\* 相同，`..._mesh_median_v1` 的 Δ\* 更大。生产 Δ=64 ⇒ 该域上现行双线性与默认档都不如「退化成帧级」的兜底臂，**只有 mesh 档胜出**。⇒ 该域的结论必须写成「在某个算子下」，不能只写「帧级更优」；
2. **未分辨结构域（cell 内 ℓ_u ≪ Δ）：没有任何重建算子能救回来**。cell 稳健尺度被 cell 内未分辨结构抬偏是主导项（该域的 E 比可分辨域高两个数量级），且该域内算子间极差小 ⇒ 算子选型在此域不是主要矛盾，控制点估计量（结构感知局部 σ）才是；
3. **M42 类真实地面帧：稠密与稀疏互有胜负**，差值量级不足以抵偿稀疏层的存储优势 ⇒ 该域只给**定性**结论（**结论口径 = 两臂互有胜负**）。

- 判据口径两条已知局限（与结论同读）：① **E 对乘性偏差完全免疫**——两个臂可以在 E 完全相同（均 0.0530）而水平偏差相差数倍（dex 量级）⇒ E 只回答「权重效率」，**不能替代水平偏差判据**；② 「帧级臂 RMSE ≤ K·s_field」类判据对任意真值场恒真（K 足够大即可），**无证据资格**。

### 4.2a σ_sky 口径冻结（防读出噪声双计）

- **唯一口径规则**：逐像素噪声 `σ_i² = σ_sky,i² + (RN/g)² + F·P_i/g`，**读噪只出现一次**。`sigma_sky_adu` 这个入参**只承载一种语义**，调用方必须显式声明是哪种；**择一依据 = 显式声明**。
- **入参语义枚举（二选一，必填）**：

| `sigma_sky_source` | 含义 | 是否再加 `(RN/g)²` |
|---|---|---|
| `shot_noise_only` | 天光 + 暗流的**散粒**方差（不含读噪） | **加**（默认路径，需 `gain_e_per_adu>0 && read_noise_e>0`） |
| `empirical_total_rms` | 经验**总** rms（含读出噪声，如生产调用点传入的 `noise_sigma` = `StarDetector::estimate_background` 的**整帧 2 轮裁剪 RMS**；噪声模型 A 的 `1.482602218505602×MAD` 稳健尺度是另一生产者，承载逐像素 `variance`） | **不加**（已含读噪） |

- **决策树（调用点实现口径）**：
  1. `sigma_sky_source=shot_noise_only` 且 `gain_e_per_adu>0 && read_noise_e>0` ⇒ 走 `σ_sky,散粒² + (RN/g)² + F·P_i/g`（设计本意路径）；
  2. `sigma_sky_source=empirical_total_rms` ⇒ 只加源泊松项 `F·P_i/g`，**不再加** `(RN/g)²`；
  3. `gain_e_per_adu<=0`（gain 未知，PSF 行路径）⇒ **源泊松项 `F·P_i/g` 同样不可加**——该项含 `1/g`，无 `g` 即无法计算 ⇒ 退回**天空受限**口径 `σ_F² = σ_sky²/ΣP_i²`，**不加** `(RN/g)²`。此口径下 SNR **系统性偏高**，是**上界**而不是绝对 SNR（解析式 `SNR_rep/SNR_true = √(1+F/σ_bg²)`，偏高幅度随背景受限程度增大）⇒ 产品必须标 `snr_caliber = upper_bound_no_gain` 并带 `snr_degraded_reason`，**该值的用途仅限上界诊断**（最高设计 §2.2 的交付物是绝对 SNR）；
  4. 声明与实际来源不一致（声称散粒而来源为经验总 rms，或反之）⇒ **fail-closed 拒绝**，**择一依据 = 显式声明**，告警不构成放行。
- **证据**（`实验/absolute-snr`：项级扫描 N_MC=1000 + 帧级 SNR 扫描）：双计臂的 `σ_F` 系统性高估，天光主导区（`B ≥ 10⁵`）偏差可忽略；正确口径臂在全部扫描点 ≤3σ。读数与复现命令见该实验单元 `results/`。
- **必须保持的不变量**：固定源通量、天光 B 增大 ⇒ SNR 单调下降、斜率 −1/2；双计会破坏该斜率。
- **负例**：`empirical_total_rms` 时再加 `(RN/g)²` 必须判红。

### 4.3 其他要点

- 白噪声时为简单形式；相关噪声时用完整信息核；
- 标量降级门：仅当帧内 `W_psf(x,y)` 鲁棒相对离散与系统趋势低于阈值才存帧级标量；否则存 map/控制点/多项式/HEALPix，摘要带 p05/p50/p95、最大系统偏差、覆盖、模型误差；
- PSF 拟合质量代理（FWHM、残差尺度等）只作诊断，**不计入科学叠加权重**；frame_snr 对标的是 PSFSNR（未加权原始信噪比）。

### 4.4 噪声模型：A 为唯一生产模型

- **噪声模型 A 为唯一生产模型**：实现 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp`，接口 `snr_noise_model_v1` / `_f64` / `_fill` + `NoiseWeightModelV1`（**逐像素**），编入根 `CMakeLists.txt` 的 `astrocs_phase1_noise`（STATIC），经 `astrocs_phase1_session` 的 PUBLIC 闭包（`CMakeLists.txt`）链入主程序。
- 模型定义、参数语义、稳健噪声估计与掩膜规则的正本见 `docs/science/NOISE_MODEL.md`；本文件只登记插件侧接口与接线事实，不复制公式。
- **逐像素方差接线现状（如实登记）**：A 已编入 `astrocs_phase1_noise` 并链入主程序；生产调度路径**已挂** `variance` 帧内命名块——`lib/infrastructure/scheduler/src/module_adapters.cpp`（`p1_op_drizzle`）按 `NoiseWeightModelV1` blank-sky variance 经 `snr_noise_model_v1_fill` 填面后 `aio_frame_add_block(frame, "variance", AIO_BLOCK_FLOAT32, …)`（`module_adapters.cpp`），引擎侧 `sumVarNum += v·w²`，sink/writer finalize 出 variance/ivar 子产品；`uncertainty_available` 为 provenance 判定结果（由磁盘事实给出，`true` ⇒ variance|ivar 位同时置位），显式降级非静默（带 `var_status`/`var_reason`）；口径正本 = `08_drizzle.md` §7。
- **逐像素方差面的两态约束（必须成立）**：`snr_noise_model_v1_fill` 输出的每一像素必须落在两态之一 —— **可用** `variance>0 ∧ isfinite(variance) ∧ ivar=1/variance`；**不可用** `variance=0 ∧ ivar=0`。
  平面预测 ≤ 0、或生效 floor / 输出值在 float32 中不可表示（下溢为 0 / 上溢为非有限）的像素取不可用态；不可用方差的落盘值 = 显式不可用态本身——
  floor clamp 会把「模型在此处失效」发布成 `ivar=1/floor` 的极大权重；`(0, +inf)` 这类自相矛盾的对同样按不可用态处理。
  正本 = `docs/science/NOISE_MODEL.md` §5/§7/§9 与 `docs/science/DATA_SEMANTICS.md` §4a 三态表；
  门 = `ctest -R p1noise_negative`（`n7_plane_pred_unavailable` / `n7b_dtype_underflow_pair`）+ `ctest -R p1noise_selfcheck`（证明判据能红，非恒真）。
- **影响面**：**不影响**帧级 SNR 路径（`snr_chain_closure="closed"`，科学上正确）；逐像素**不确定度产品面**由生产调度路径经 A 的插件路径产出（接线在场：`module_adapters.cpp`）。凡「逐像素方差/不确定度已传播到产品」的主张**必须**附 `n_variance_tiles>0` 的磁盘证据。

### 4.4a 背景方差面的自适应拟合与 §5d 审计面

背景方差面 `var(x,y) = a + b·x + c·y` 的**控制点有效性**与**拟合可行域**都由数据自身给出，
**不含按数据集标定的常数**（正本 = `docs/science/NOISE_MODEL.md` §5d；实现 = 噪声模型 A 的
`snr_noise_model_v1` / `_f64` 构建路径）。

- **控制点有效性判据是自校准统计量**：`R = σ_MAD / σ_white-equiv`——分子是该 patch 的稳健尺度，
  分母是同一 patch 在**白噪声零假设**下的等价尺度。该判据的**零假设值恒为 1**，是恒等式而不是标定值
  ⇒ **`R` 的比较基准 = 零假设值 1 本身**；与写死的绝对倍数（如 `R > 3`）比较属另一口径。
- **判据实现 = `ln R` 的序统计量**：取最大的 `k` 使第 `k` 个跳变超过保留主体的极差；
  `min_keep = max(⌈n/2⌉, 预算 patch 数)`；`n < min_keep + 1` 时判据**不武装**，
  这些 patch 计入 `n_r_unavailable_patches`，**按不可用登记**。
- **拟合可行域**：平面必须在**控制点凸包内结构非负**（凸包内预测 ≤ 0 是拟合缺陷，不是合法外推）；
  **合法解集 = 凸包内结构非负的平面**（常数场属另一形态）。
- **拟合权重 = 相对误差加权**：`w_i = (v_med / v_i)²`（`v_med` = 控制点方差的中位数）。
- **可观测量必须写入 provenance**，缺任一必落字段即该帧方差面**不可审计**：
  `<帧>/p1_final.json#variance_audit`（内容逐字读回 `<帧>/p1_stack.json#variance_audit`）。
  必落字段 = `ctrl_variance_range`、`plane_a` / `plane_b` / `plane_c`、
  `hull_nonpositive_frac`、`n_structure_rejected_patches`、`r_min` / `r_median` / `r_max` / `r_fence`；
  逐帧另有 `variance_audit_present` / `variance_audit_available` /
  `variance_audit_missing_fields` / `variance_audit_source`。清单的机器唯一源 =
  `astrocs::noise::variance_audit_required_fields()`（生产者、校验者与测试**必须**引用同一份）。
- **失败语义**：本帧**已发布** variance/ivar 子产品而必落字段不齐 ⇒ **fail-closed**（DATA 类，
  `p1_final.json` 不落盘）；`hull_nonpositive_frac != 0` ⇒ 该方差面不可审计。
  未发布方差面时如实登记「本次没有这张面」，**「无此产品」与「字段丢了」各按各自语义读**。

### 4.5 稀疏帧内层几何与重建算子

- 稀疏控制点间隔 Δ 复用 Phase2 UPM 的 8×8/tile 控制网格，`Δ = hips.tile_width / 8`（`hips.tile_width = 512` ⇒ Δ = 64 px）；默认保持 Δ=64 px（HST 类高对比数据推荐 32 px，下限 16 px），取值依据、失效边界与完整适用域正本见 `实验/absolute-snr`（EXP-04 §5）；
- **几何（冻结）**：控制点坐标是**像素中心坐标**（像素序号 p 对应坐标 p）；规则网格下每个控制点落在**所属 Δ×Δ cell 的中心**——cell i 覆盖像素 `[origin_x + i·Δ, origin_x + (i+1)·Δ − 1]`，其中心 `= origin_x + i·Δ + (Δ−1)/2`（y 同）。这与 Phase2 UPM 控制网格是同一约定（`lib/algorithms/coverage/src/upm.cpp` 的 centered bilinear basis：控制观测与系数节点位于 cell 中心；角点求值会产生半 cell 相位偏移，实现以 cell 中心求值规避）。**把节点当 cell 角点会使重建场整体平移半个 cell（Δ=64 时 31.5 px）**；cell 网格原点由 `grid_origin_x/y` 显式声明（缺省 0 = 帧原点），消费侧以「节点—cell 中心」一致性门 **fail-closed** 拦截该错位；
- **定义域 = 层覆盖的 cell 并集**（`[origin_x − 0.5, origin_x + nx·Δ − 0.5]`，y 同）：cell 内非节点处由重建算子插值给出，最外半个 cell 由端点节点常数延拓；越出该并集即 **fail-closed**（不外推、不回退帧级）；
- 稀疏控制点存**绝对** SNR（与帧级 SNR 同口径、同逐帧参考通量 `F_ref`）；重建算子在控制点上重建出稠密绝对 SNR 场；帧级标量与稀疏层相互独立，不作其尺度基准，消费时也不参与还原。
- **重建算子（冻结词表；算子标识 = 唯一配置面）**。算子标识把「核 + 是否开 3×3 mesh 中值前置滤波 + 是否做值域钳制」**整组绑定**，不做成可自由组合的独立开关：钳制是正值与有界性的必要条件，滤波只在特定域必需，独立布尔可组合出从未实测的配置（如双线性+滤波、无钳制样条）。算子标识入 manifest（`SparseReconstruction.operator_id`）。

| 算子标识 | 语义 | 定位 |
|---|---|---|
| `natural_bicubic_spline_clip_v1` | 可分离**自然边界**双三次样条 + **钳到有效控制值值域** | **默认**（地面/seeing-limited、解析可分辨域与一般情形） |
| `natural_bicubic_spline_clip_mesh_median_v1` | 同上，且在样条前插入 **3×3 mesh 中值滤波**（边界 replicate、无条件替换） | **高对比域开关**：cell 内含未分辨亮源（HST/空间高分辨率） |
| `bilinear_regular_grid_v1` | 规则网格双线性（边界 clamp） | 保留为**对照/回退**（现行实现的忠实保留） |
| `nearest_control_point_v1` | 最近控制点 + 显式覆盖半径 | 散点层唯一合法算子 |

- 算法（默认路径，全部 O(N)）：① 无效控制点（NaN，即 schema 的 `invalid_repr`）取最近有效控制点（等距并列取平均），**全部无效 ⇒ fail-closed**；② 可分离自然边界双三次样条（y 向 natural BC → x 向 natural BC）；③ 钳到有效控制值值域 `[min(ctrl), max(ctrl)]`。高对比档在 ① 与 ② 之间插入 3×3 mesh 中值滤波。非 NaN 的非有限值与 ≤0 是**非法值**（不是 invalid 表示）⇒ fail-closed；未识别算子标识 ⇒ fail-closed（默认档只对已识别算子标识生效）。
- **值域钳制不可省**：去掉钳制后，光滑插值类在病态控制网格上的权重效率损失 E 上升若干数量级，并会给出**负的 σ**（非物理）；正常面上钳制的代价可忽略。读数见 `实验/absolute-snr`。
- **mesh 中值滤波是必需项而非可选优化，且必须按域显式开启**：HST 类高对比域上默认路径单独使用劣于帧级兜底，叠加滤波后转为胜出；而在解析可分辨域与 M42 类真实地面帧上滤波造成显著损失。⇒ **默认关**；读数见 `实验/absolute-snr`；
- **选择规则（按数据来源，不按控制网格自身推断）**：可判定 cell 内含未分辨点源（空间高分辨率 / HST 类）⇒ 声明 `..._mesh_median_v1`；地面 seeing-limited 与一般情形 ⇒ 默认档；**无法判断 ⇒ 默认档**（默认目标域上滤波有害）。该规则由 `sparse_recon_operator_for_source()` 显式承载。**从控制网格自身推断不可行**：两个候选标量诊断（相邻控制值差分比 ρ、`std(log10 ctrl)/ε_cell`）都不能把「滤波有益」与「滤波有害」的域分开，且在 16-bit 整数真实数据上直接失效（EXP-04 §4.3）；
- 任何算子都必须满足四条**与选型无关**的约束：① **显式声明**——算子标识入 manifest；② **正齐次性** `R[a·v] = a·R[v]`（`a > 0`）——控制点值整体缩放时重建场按同一因子缩放，这是「帧内共模因子在权重口径下相消」成立的前提，带**数据无关固定先验均值**或向固定值收缩正则的算子（如固定先验均值的 GP/kriging）**不满足**该条；③ 在控制点处**精确复现**节点值；④ 越出定义域即 **fail-closed**，域外取值一律显式拒绝（外推/帧级回退各属另一路径）。不正齐次的算子若被选用，必须把 `homogeneity` 声明与「该算子下相对场**不能**由绝对场缩放得到」写入 manifest，消费侧据共模相消做的等价假设**以该声明为前提**（算子正齐次性的反例与残差量级见 `实验/absolute-snr` EXP-05 §3.1/§3.2）；
- 控制点位置、取值、采样覆盖、`reconstruction_operator` 与 `snr_path_effective` 一并写入 manifest 与产品内容证据块。
- **插值设置配置化＋运行日志输出，不随产物落盘**：词表只冻结算子标识与语义（上表）；算子级数值设置属**配置级**参数，由配置承载、不冻结进词表——备选 IDW 口径保持**备选定位**（不在上列冻结词表内），其 `idw_power` 默认 **1.0**（适用域 = 含噪场景，最优带 0.5–1 的上端；p=2 属无噪/光滑极限口径）、`K` = 16、重合点守卫 γ<1e-10，以及 mesh 滤波开关等，均按配置解析；**实现侧接线现状（如实登记）**：`snr_evaluator` 的兜底默认**与规范一致为 1.0**（`snr_evaluator.h` 成员初始化 `idw_power_ = 1.0`；`snr_evaluator.cpp` 两处 ≤0 守卫均回落 `1.0`），配置显式传值时不触发兜底；每次重建把**实际生效**的插值设置写入**运行日志**（IDW 口径含实测最优 `p*`（argmin）、`K` 与噪声档），**不随产物落盘**——产物面只按上条登记算子标识与层几何（manifest）。`p*` 依赖场形态与噪声档，不冻结单一「最优值」，随日志积累重标定。
- **控制点局部 σ 必须结构感知（数值准确的必要条件）**：控制点存绝对 SNR 只保证**表示正确**，不保证**数值准确**——后者完全由局部 σ 估计器决定。把估计作用域从整帧朴素地换成区域（同一 recipe + 更小窗口）**不够**：整帧口径的结构污染只是被挪到更小尺度，cell 内的未分辨结构仍被算进稳健尺度。控制点的局部 σ **必须**用**结构感知**估计器：mesh 局部背景扣除后的逐区域残差稳健尺度，或跨帧差分（唯一零结构偏差口径）；估计器标识、`sigma_rho` 与 `quality_flags` 一并入 manifest。估计器认证、逐区域偏差与适用域见 `docs/science/CONTROL_WEIGHT_SNR.md` §8b 与 `实验/absolute-snr`（EXP-03 的 R0/R1/R2）。

### 4.6 输出独立性与分面约束

- `source_snr` / `depth_m5` / `point_information` / `frame_snr` / `sparse_snr_layer` 各是各的对象，各有独立 schema，取值互不代用；
- `point_information` 是点源严格权重（`1/Var(F_hat)`），与帧级 SNR 是不同对象：前者是权重、后者是信噪比；
- 权重不落盘：HiPS 里只存帧级 SNR 与稀疏**绝对** SNR，权重由 Phase2 集成时现场派生（最高设计 §3.1）；
- 端口只接同一种对象：Phase2 集成端口只接受 `frame_snr` / `sparse_snr_layer` 与 `point_information`，接错判红（最高设计 §3.1）；
- Phase1 与 Phase3 不产生、不消费权重；全程只有 SNR。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `reference_flux` | 逐帧 `F_ref,k` | 见说明 | 参考通量（m5/SNR 定义必需）。**逐帧**：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，`m_ref = 6.0`，`reference_flux_scope = frame_independent_fixed_magnitude`；公共锚 `F0` 满足 `F_ref,k · k_photo,k = F0`。单位以产物字段 `reference_flux_common_unit` 为准（现行 = `F_syn (Gaia XPSD absolute spectral integral)`；显式 `snr.reference_flux_adu` 覆盖时为 ADU）。⚠ `m_ref = 6.0` 是**参考电平的形式外推**，**用途限定为参考电平**（6 等星档） |
| `scalar_gate_rd` | —— | —— | 标量降级鲁棒离散门 |
| `scalar_gate_trend` | —— | —— | 标量降级系统趋势门 |
| `psfsw_enable` | true | —— | 是否产出 PSF 信号权重复合分量（诊断/基线对照；PSF 拟合质量代理不计入科学叠加权重） |
| `sparse_snr_layer` | true | —— | 是否产出稀疏帧内 SNR 层（控制点值 = 绝对通量型 SNR `F_ref/σ_F(x,y)`，与帧级同口径、同 `F_ref`）。**默认产出**（默认稀疏路径） |
| `sparse_snr_spacing_px` | 64 | px | 稀疏层控制点间隔 Δ（px）：复用 Phase2 UPM 的 8×8/tile 控制网格 ⇒ `Δ = hips.tile_width / 8 = 512 / 8 = 64`。取值依据与适用域见 §4.5 |
| `sparse_snr_density` | —— | 点/度² | 稀疏层控制点密度（按面积表述）；生产使用像素域控制点间隔 `sparse_snr_spacing_px` 承载该量，本键不承载生产取值 |
| `snr_path` | `sparse_reconstruct` | —— | SNR 重建路径：`dense` / `sparse_reconstruct`（默认）/ `frame_reconstruct`；三条路径的适用域由 `实验/absolute-snr` 给出 |

- **重建算子的声明面不是配置键**：算子标识由稀疏层自身声明（`sparse_snr_layer.reconstruction_operator`，冻结词表见 §4.5），随层入 manifest。本表**不**登记该键——避免出现「配置一套、层里另一套」的双事实源。

## 6. 接口/ABI

- entrypoint：信号+ivar+PSF+`a_k` → {source_snr, depth_m5, point_information, frame_snr[, sparse_snr_layer]}；
- 帧级 SNR 经 drizzle 写入 HiPS 文件头；稀疏层作为标准层插入 HiPS；
- **Phase2/Phase3 不产出 SNR**：Phase2 在集成中现场消费本模块的单帧 SNR（换算逆方差权重），不输出 SNR 面；Phase3 无 SNR；
- 各类输出独立 schema，各自独立成面。

## 7. 错误与边界

- **重建算子与层几何（能红能绿）**：未识别的算子标识 ⇒ fail-closed（默认档只对已识别算子标识生效）；声明为规则网格算子而层是散点、或反之 ⇒ fail-closed；控制点不在所属 cell 中心（角点锚定的控制网格，半 cell 相位）⇒ fail-closed；控制值为 0/负/±inf（非法值，不是 invalid 表示）⇒ fail-closed；控制点为 NaN（= schema 的 `invalid_repr`）⇒ 按最近有效控制点填充（等距并列取平均）并把填充数入 manifest，**全部无效 ⇒ fail-closed**；查询点越出层覆盖的 cell 并集 ⇒ fail-closed（`out_of_domain`，不外推、不回退帧级）；
- 缺 `a_k`/PSF/方差 → fail-closed（信息权重不可凭空造）；
- 标量门失败 → 自动升级为空间模型（标量结果只保留具名降级登记）；
- `reference_flux` 未定义时 m5/SNR 不可输出；无 `photscale_fit` / `zero_point_valid=false` ⇒ 按显式回退 `group_median`（`reference_flux_scope` 落盘，**不伪造**），显式 `snr.reference_flux_adu` 优先；**逐帧中位数回退为 fail-closed，该形态保持**；
- **组内 `F_ref` 相等只作登记事实**；缺 `variance` 块 ⇒ `uncertainty_available=false`，**逐像素方差的产出主张以 `variance` 块在盘为准**（§4.4）；
- 帧级 SNR 无法计算（如缺真实信号参考）→ fail-closed；受天光影响的普通 SNR 属另一个量，两者互不代用；
- 指定 `sparse_reconstruct` 路径而输入无稀疏层 → 按帧级执行并**显式记录实际路径**（不静默）；稀疏层损坏/不可重建 → fail-closed；
- **稀疏层值语义判红**：把控制点值按**相对因子**解释（含乘/除帧级标量做还原）⇒ 必须判红。控制点值是**绝对**通量型 SNR，由 schema 的 `sparse_snr_semantics` 冻结为 `absolute_flux_type_snr`；声明为相对语义或缺失该键 ⇒ schema 判红；
- 任何信号项未扣局部背景、被天光/背景抬高的 SNR → 判红（红线 §4.1）；
- **§5d 审计面判红（§4.4a）**：本帧**已发布** variance/ivar 子产品而 `variance_audit` 必落字段不齐 ⇒ 判红（DATA 类 fail-closed，`p1_final.json` 不落盘）；`hull_nonpositive_frac != 0` ⇒ 该方差面不可审计。本帧未发布方差面时如实登记「本次没有这张面」，**「无此产品」与「字段丢了」各按各自语义读**；
- **σ_sky 双计判红（§4.2a）**：`sigma_sky_source=empirical_total_rms` 时再加 `(RN/g)²` ⇒ 必须判红（保护负例）；
- **声明义务**：**生产调用点**（`module_adapters` 的 `p1_op_noise`）必须显式声明 `sigma_sky_source`，缺失即评审判红；声明与实际来源不一致（声称散粒而来源为经验总 rms，或反之）⇒ fail-closed 拒绝。
- **C ABI legacy 路径**：`SNR_SIGMA_SKY_UNSPECIFIED=0` 保留为直接 C API 调用方的兼容缺省（与 `SHOT_ONLY` 组合**逐位一致**，由 `p1snr_science_skysource` 的向后兼容锁固定）；该路径的适用范围 = 直接 C API 调用方（生产链走显式声明）——「缺失即 fail-closed」由调用点层（而非 C 函数层）保证，因为 C 函数层无法观测入参的**实际来源**。

## 8. 测试与 Oracle

- 注入点源：理论 `σ_F=1/√W_psf` 与实测散度一致；
- 改变星表亮度分布不改变 `W_psf`、但改变 median source SNR（跨模块验证）；
- seeing/背景/透明度按理论改变信息权重；
- **加性背景平移不改变信号项**（注入恒定背景偏置，测光信号不变）；**天光散粒噪声增强时 `σ_n` 增大、帧级 SNR 按理论下降**；**单调性负例**：固定源通量、天光 `B` 增大 ⇒ SNR 单调下降，`B→∞` 时 `SNR→0`；
- **注入-回收**：已知真值 `F_s` + 已知天光 + 已知噪声 ⇒ 回收 SNR = `F_s/σ_F`（三条路径各一组；不满足者判红）；
- 稀疏层：启用/不启用输出结构正确，稀疏层值可重建验证；**三路径精度与存储量对比**：`dense` / `sparse_reconstruct` / `frame_reconstruct` 同输入重建稠密 SNR，报告精度差与存储量；无稀疏层而路径为 `sparse_reconstruct` 时实际路径须被显式记录（负例：静默降级判红）；
- **重建算子 Oracle（能红能绿，`lib/algorithms/integration/phase2_integrate/oracle/`）**：① 正例——默认算子与独立复算的自然样条+钳制逐点一致、控制点自身复现残差 ~0、预置路径与单次调用逐位一致、1/8 worker 求值逐位一致；② 与实验单元 EXP-04 的算子实现逐像素对拍（容差 1e-12）；③ 负例注入——移除值域钳制 ⇒ 病态网格 E 爆增判红；把 mesh 滤波档设为全局默认 ⇒ 默认目标域上默认档与滤波档持平判红；移除 cell 中心几何门 ⇒ 角点锚定网格被接受判红；同时移除钳制与正值守卫 ⇒ 重建场出现负 σ 判红；
- **稀疏层绝对语义判据（能红能绿）**：① 绿——控制点自身复现：在节点坐标处重建值 == 落盘控制点值（残差 ~0），且与帧级标量**无关**（同层配不同帧级标量，重建结果逐位相同）；② 红——按相对值解释：取 `SNR(x,y) = frame_snr × v(x,y)/median(v)` 时，节点重建值 ≠ 落盘值（除非 `frame_snr == median(v)`），该差异必须被判红；③ 红——schema 层：`sparse_snr_semantics` 声明为相对语义或缺失 ⇒ 合同测试判红；
- 点源/面亮度口径分离：把面亮度 SNR 当帧级 SNR 使用必须判红；
- **逐帧 `F_ref,k` 独立性负例**：人为要求组内 `F_ref` 相等（组间硬闸门）⇒ 必须判红——不同指向/不同光学系统的帧合法地有不同 `F_ref,k`；`w_k = SNR_k²/F_ref,k²` 的配对性**只在同一帧内**成立；
- **背景方差面判据（能红能绿，§4.4a）**：注入结构（未分辨源 / 强梯度）⇒ 被 `ln R` 序统计量判据剔除、`n_structure_rejected_patches > 0`；真值无结构 ⇒ 判据**不动作**（`n_structure_rejected_patches == 0`、`r_median ≈ 1`，证明判据非恒真）；`R` 不可算的 patch 计入 `n_r_unavailable_patches` 而**不**被剔除；
- **§5d 审计面**：正常帧 `variance_audit` 必落字段齐全、`hull_nonpositive_frac == 0`、`ctrl_variance_range > 0`；删掉整块或只删单个字段后重跑 writer ⇒ 判红（负例）；
- 1 worker vs N worker 一致。
