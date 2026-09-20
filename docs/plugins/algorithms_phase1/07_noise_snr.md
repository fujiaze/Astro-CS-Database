# 插件文档：noise_snr（噪声/SNR/信息权重）

## 1. 职责与边界

- **职责**：从校准方差、背景、PSF 与光度响应估计逐像素噪声、逐源 SNR、深度 `m5`、点源信息权重 `W_psf` 与**帧级 SNR（信噪比）**（写入 HiPS 文件头的唯一帧级参考）。**Phase1 的 HiPS 是唯一带 SNR 数据块的产物**（帧级 + 稀疏区域）。
- **不是**：不生产"一个模糊的 snr 字段"；不把 median source SNR 当科学权重；不把 PSFSW 复合权重冒充 Fisher information；**不产出外挂独立 SNR 文件**（帧级 SNR（信噪比）写入 HiPS 文件头）；**不替 Phase2/Phase3 产出 SNR**（Phase2 在叠加中消费单帧 SNR、不输出 SNR 面；Phase3 无 SNR）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §3.3（输入合同）、§3.4（输出合同：帧级 SNR（信噪比）入文件头）
- `docs/design/UNIFIED_MODEL.md`（数据对象表：frame_snr、sparse_snr_layer）
- `docs/science/NOISE_MODEL.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`
- `docs/design/PHASE1_DETAILED_DESIGN.md` §8（SNR、PSF Signal Weight）
- `docs/research/SNR_WEIGHT_RESEARCH_PACK.md`（PixInsight 公开方法学、开源对照实现与文献的研究任务包）

## 3. 输入/输出数据合同

- **输入**：定标信号、variance/ivar、validity、PSF 模型/地图、光度响应 `a_k`、检测目录。
- **输出**（独立对象，禁止混装）：
  - `source_snr`：`SNR_s = F_hat_s / σ_F,s`（源测量诊断，依赖源亮度）；
  - `depth_m5`：`m5 = ZP − 2.5log10[5σ_F(ref)]`（帧/位置深度表达）；
  - `point_information`：`W_psf(x,y) = a²PᵀC⁻¹P = 1/Var(F_hat)`（点源严格权重）；
  - `psfsw_robust`：四分量（signal/concentration/robust noise/robust background）+ 相对权重 + validity + 共同星集/selection function；
  - **`frame_snr`**：帧级 SNR（信噪比），**写入 HiPS 文件头**；语义 = **点源（PSF）信号 SNR**（纯信号/噪声，红线见 §4.1）；
  - **`sparse_snr_layer`**（`sparse_snr_layer=true` 时）：帧内稀疏控制点 SNR 层，作为标准层插入 HiPS 文件内；**本期决议默认产出**（默认稀疏路径，§4.2）。
  - **不输出**：Phase2/Phase3 产物不含 SNR 面/块；Phase2 只在叠加中消费本模块产出的单帧 SNR（现场换算逆方差权重），**不直接复用**本模块的 SNR 产物。
- 参考：`contracts/schemas/noise_snr_output.schema.json`。

## 4. 算法与公式要点

```text
W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k = 1/Var(F_hat_k)
白噪声: W_psf,k = a_k² Σ_p P_k,p² / σ_pix,k² = a_k² / (σ_pix,k² A_NEA,k)
SNR_k²(F_ref,k) = F_ref,k² W_psf,k
```

### 4.1 帧级 SNR（信噪比）（frame_snr）

- 是**唯一帧级参考**，写入 HiPS 文件头；
- **是未加权的原始信噪比，不是权重**——它只描述"这一帧的真实源信号相对真实噪声有多强"这一客观测量事实，不包含任何为某次叠加服务的加权；权重由 Phase2 逆方差叠加时从 SNR 现场计算（见最高设计 §4.3）；
- **可靠且独立**：必须是**真实的信号与噪声比例**，信号项不被加性天光背景虚高——普通 SNR（全局信号方差/噪声方差，含天光背景）随天光变亮而虚高，不能作唯一帧级参考；
- 天光的物理影响如实计入噪声：信号经独立的局部背景估计与扣除获得，不随加性背景平移而虚高；但天光散粒噪声是真实噪声的一部分，天光变亮会增大 σ_n、降低 SNR，这是真实物理变化而非指标漂移。

**帧级 SNR 红线（负责人 2026-09-19 裁决 A1/C1，claim `FIX-SCI-SNR-CANON-001`；不可协商）**：

- `SNR = F_signal / σ_F`，`F_signal` **必须已扣局部背景**；天光**只作为噪声项**进入 `σ_F`；
- **必须可证明**：固定源通量、增大天光 ⇒ SNR **单调下降**（`B→∞` 时 `SNR→0`）；
- 帧级 SNR 是**点源（PSF）**量，**不得**与面亮度 SNR 混用或互相宣称等价；
- **验收方式（C1）**：对每条 SNR 路径做**注入-回收**——已知真值信号 + 已知天光 + 已知噪声，回收的 SNR 必须等于真值 `F_s/σ_F`；不满足者否决或修正；
- 具体定义式（`F_signal` 的测光口径与 `σ_F` 的稳健噪声估计）**待 FRAME-SNR-CANON 文献调研结论补入**；本文件先固化红线与方向，不预设某一种方法。

**与 PixInsight 公开方法学的关系（精确对标）**：PixInsight 核心闭源，但其《New Image Weighting Algorithms》参考文档公开了完整方法学，其中是**两个不同的量**，必须区分：

| 量 | 定义（官方） | 性质 | AstroCS 对应 |
|---|---|---|---|
| **PSFSNR** | ratio-of-powers 信噪比（官方文档式[18]）：`c3·(Σ_j f_j)² / (c4·σ_n²)`，分子是**和的平方**，f_j 为各星 PSF 通量（FWTM 孔径内像素减局部背景求和），σ_n 为稳健噪声（MRS/N*）；c3、c4 是其模拟数据标定常数（文章版 c3=1.350×10⁻⁷、c4=4.987×10⁺⁶，PCL 2.10.4 为 c3=1.316×10⁻⁷，随版本漂移） | **未加权的原始信噪比**（功率比口径，量纲为 SNR² 量级） | **frame_snr 对标此量的方法学**（信号取数、稳健噪声、独立背景三点），数学定义采用下节通量型口径以保证逆方差换算严格成立，不逐字套用此式，独立标定常数、不照抄 c3/c4 |
| **PSF Signal Weight（PSFSW）** | 综合**图像质量权重**：信号总量 × 信号集中度（mean flux，随 FWHM 变小而增大）/（稳健噪声 × 稳健平均背景 M*） | **权重**，额外含分辨率/FWHM 与背景梯度惩罚，不是信噪比 | 仅显式 `weight_mode=psfsw_robust` 时使用，四分量独立存储 |
| 标准 SNR | `σ²/σ_n²`，全局尺度估计 | 信噪比，但**受天光/梯度正向影响**，官方明确指出它会给目标 SNR 很低的亮背景帧虚高权重 | 不采用 |

- 共同的、我们借鉴的方法学：① 信号只从检测到的恒星经 PSF/孔径混合测光得到（不用拟合振幅，只用采样像素减独立估计的局部背景）；② 噪声用稳健多尺度估计；③ 背景（天光）作为**独立的稳健分量**估计与扣除，不进入信号——这三点保证 frame_snr 的信号项不被天光背景虚高（天光散粒噪声仍计入 σ_n）；
- PixInsight 官方同样**不把权重存进图像**：校准阶段只把信号/噪声/背景分量写入元数据（FITS 关键字 PSFFLX/PSFMFL/PSFMST/PSFNST/NOISE 等），权重在 ImageIntegration 集成时才计算——与 AstroCS"数据库存原始 SNR、Phase2 消费时才算权重"的设计一致；
- PSFSW 归一化常数（c1/c2）与 PSFSNR 常数（文章版 c3=1.350×10⁻⁷、c4=4.987×10⁺⁶；PCL 2.10.4 源码 c3=1.316×10⁻⁷，存在版本漂移，引用须带版本）均由 PixInsight 自造 1000 张 4096² 模拟图标定，**AstroCS 不照抄**：采用无量纲/物理量纲定义，常数由本项目合成数据（验收 L1）独立标定并冻结。

**数学定义与换算**（**逐帧**参考通量 `F_ref,k` + 公共锚 `F0`，见 `docs/science/PSF_SIGNAL_WEIGHT.md`；基准定案 §9.60 `FREF-BASELINE-001` / §9.67 定案 4）：

```text
# 帧级 SNR（未加权原始信噪比，写入文件头）：真实源信号 / 真实噪声（通量型口径，Horne 1986）
SNR_k(F_ref,k) = F_ref,k · sqrt(W_psf,k) = F_ref,k / σ_F,k
W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k  （点源信息，σ_F,k² = 1/W_psf,k；信噪比本身未做任何加权）
F_ref,k = 10^(−0.4·(m_ref − ZP_k))，m_ref = 6.0；F_ref,k · k_photo,k = F0（公共锚，严格恒等）

# Phase2 叠加时现场换算为逆方差权重（UPM 已归一到公共通量尺度）：
w_k = 1/σ_F,k² = SNR_k(F_ref,k)² / F_ref,k²   ⇒  w_k ∝ SNR_k²（配对性只在同一帧内成立）
```

- **逐帧 `F_ref,k`（不是组内公共常数）**：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))` 由**该帧自己的** `ZP_k` 决定，`m_ref = 6.0`，`reference_flux_scope = frame_independent_fixed_magnitude`（§9.60 `FREF-BASELINE-001`）；公共锚 `F0 = 10^(−0.4·(m_ref − ZP_syn))` 与帧无关，严格恒等 `F_ref,k · k_photo,k = F0`（实测偏差 ≤ 4.3e-4）。
  **旧写法「`F_ref` 为组内公共常数」已作废**（§9.49 定案 2 帧间独立；§9.66 A 变更 claim `WEIGHT-FREF-PERFRAME-001`）：配对性定理只要求**同一帧内** SNR 与 `F_ref` 同源，**不要求跨帧相等**；不同指向/不同光学系统的帧**合法地**有不同 `F_ref,k`（旧的组间 `F_ref` 硬闸门已删除——它曾使 `weight_mode=2` rc=2）。
- **`m_ref = 6.0` 适用域警告**：`ZP_syn` 由 Gaia DR3 XP 绝对 XPSD 谱经本帧滤光片/QE **正向合成**，`F_ref,k` 是在**线性区外**的形式外推（实测：M42 Red 300 s 超饱和 1899×；HST M16 F657N 超 WFC3/UVIS 满井 1.9e4×）。作为**参考电平仍良定义**（天光限下 `SNR ∝ F_ref`），但**不得**表述为「本帧能测到的 6 等星」（§9.60 需前台处理项 2）。
- **口径澄清（与 PixInsight 式[18]的区别）**：上式 frame_snr 是**通量型**信噪比 `F_ref,k/σ_F,k`（一次方比），逆方差换算 `w_k=SNR_k²/F_ref,k²=1/σ_F,k²` 严格成立；PixInsight 式[18] PSFSNR 是**功率比型** `(Σf)²/σ_n²`（平方比，本身已是 SNR² 量级），不能再做 `SNR_k²/F_ref,k²` 换算。AstroCS 只对标 PSFSNR 的方法学（恒星测光取信号、稳健噪声、独立背景），数学上采用通量型口径以保证与逆方差叠加严格自洽；

- HiPS 是数据库：帧产品长期保存、可被任意多次、任意科学目标的叠加消费，因此入库的是客观的未加权 SNR（与具体集成无关的观测量），把"选哪种权重模式"留给 Phase2；
- Phase2 默认 point_information 逆方差叠加；`weight_mode=psfsw_robust` 时使用 `psfsw_robust` 复合权重（信号/集中度/稳健噪声/稳健背景四分量独立存储，模式显式选择，不自动切换）；
- 稀疏帧内层启用时，每个控制点同样存未加权 SNR(x,y) 而非权重。

### 4.2 SNR 三条路径与稀疏帧内层（负责人裁决：默认稀疏）

**三条路径全部保留在算法面**（Phase1 产出 / Phase2 重建），由配置文件 JSON 显式指定：

| 路径 | Phase1 产出 | Phase2 重建稠密 | 定位 |
|---|---|---|---|
| `dense` | 稠密逐像素 SNR 面 | 直接使用 | 精度基准（论文对照组） |
| `sparse_reconstruct`（**默认**） | 帧级 + 稀疏控制点 SNR 层 | 由稀疏层重建 | 存储/精度折中（负责人选定默认） |
| `frame_reconstruct` | 仅帧级标量 | 由帧级重建 | 单帧级对照（论文对照组） |

- **默认 = `sparse_reconstruct`**：`sparse_snr_layer=true` 时生成稀疏控制点 SNR 层，作为**标准层插入 HiPS 文件内**；
- 实际 SNR = **帧级 × 帧内**（SNR 是信噪比，不是权重）；
- 稀疏层的位置/值/采样覆盖写入 manifest；
- **论文核心实验（判据 SP-0）**：同条件比较三条路径重建稠密 SNR 的精度；**不得预设稀疏一定最好**——实测同条件帧上帧级标量已最优到 0.06%（稀疏层净亏 3.3%），「何时哪种最优」须由实验回答；
- **不静默降级**：输入无稀疏层而路径为 `sparse_reconstruct`（含默认）⇒ 按帧级执行但**必须显式记录实际路径**（`snr_path_effective`）并计数；稀疏层存在但损坏/不可重建 ⇒ 显式失败；
- 存储量/精度折中与显式指定口径见 §5 配置项。

### 4.3 其他要点

- 白噪声时为简单形式；相关噪声时用完整信息核；
- 标量降级门：仅当帧内 `W_psf(x,y)` 鲁棒相对离散与系统趋势低于阈值才存帧级标量；否则存 map/控制点/多项式/HEALPix，摘要带 p05/p50/p95、最大系统偏差、覆盖、模型误差；
- PSFSW 是综合图像质量**权重**（含 FWHM/背景梯度），无量纲、组内相对、可驱动显式 `weight_mode=psfsw_robust` 集成，**不是信噪比、不是 ivar**；frame_snr 对标的是 PSFSNR（未加权原始信噪比）而非 PSFSW。

### 4.4 噪声模型双实现登记（§9.66 B / §9.67 定案 3；**未闭合**）

> 登记日期 2026-09-20；依据 `GAP_AUDIT.md` §9.66 B（2479–2509）与 §9.67 定案 3（2565–2568）。

- **仓库内存在两套噪声实现**，而**生产调度路径用的是没有逐像素能力的那一套**：

| | 插件/编排路径 | **生产调度路径** |
|---|---|---|
| 实现 | `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` | `lib/algorithms/noise_snr/wrapper_phase1/noise_model.cpp` |
| 接口 | `snr_noise_model_v1` / `_f64` / `_fill` + `NoiseWeightModelV1`（**逐像素**） | `astrocs::phase1::NoiseModel`（**标量**） |
| 编入 | `astrocs_p1_noise`（SHARED，`lib/algorithms/noise_snr/CMakeLists.txt:35`） | `astrocs_phase1_noise`（STATIC，根 `CMakeLists.txt:620`） |
| 调用者 | `orchestrator.cpp:4694–4839`（DLL 加载，挂 `variance` 块 → V19 方差/ivar 产品） | `module_adapters.cpp:4432`（**只挂 `data` 块**） |

- **链路后果**：生产路径从不挂 `variance` 块 ⇒ `hp_drizzle_api.cpp:1024` 读不到 ⇒ `astro_sphere_sink.cpp:306–327` 不产出 variance/ivar 子产品面 ⇒ `p2_integrated.json` 报 `uncertainty_available=false`（原因 `ivar_product_missing_frame_snr_fallback`）。
- **裁决（§9.67 定案 3）**：**不是「哪套在跑就用哪套」**——须先做**公式级比对**（blank-sky 稳健方差、MAD→σ 常数、Poisson 项、饱和掩膜、平面场、scale law），**判定哪一套科学正确、用正确的那一套**，另一套删除或登记退役。
- **接入义务（§9.67 定案 2）**：把 `orchestrator` 的**逐像素方差接线**（`orchestrator.cpp:4694-4839`）搬进 `scheduler`，使生产产出逐像素 `variance`/`ivar` 产品面；接入并验证后**删除** `lib/infrastructure/pipeline/orchestrator/cpp/` 与 v6 家族。
- **架构选择须上呈（§9.66 B，属 `AGENTS.md` §9 第 1 类）**：补齐生产路径需在 ①编入静态库（可能重复符号/两套语义）/ ②加载插件共享库（改 ABI 与加载时序）/ ③在 `wrapper_phase1` 重新实现（**两套科学公式副本**，违「零公式副本」纪律）之间选型 ⇒ **不在本包内擅自选型**。
- **影响面（诚实）**：**不影响** `weight_mode=2` 权重链（帧级 SNR 路径，`snr_chain_closure="closed"`，科学上正确）；**影响**逐像素**不确定度产品面**。凡「逐像素方差/不确定度已传播到产品」的主张，**只能引用插件路径证据**，**不得声称生产路径已产出**。
- **状态**：**未闭合**（审计 S16 登记；`wrapper_phase1` 在 `docs/plugins/` 此前 0 命中，本条即其登记位）。
- **行号说明**：本节的 `文件:行` 取自 §9.66 B 审计时点（2026-09-20）；工作树并发改动会使行号漂移，**以符号名/文件名核对为准**。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `reference_flux` | 逐帧 `F_ref,k` | 见说明 | 参考通量（m5/SNR 定义必需）。**逐帧**：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，`m_ref = 6.0`，`reference_flux_scope = frame_independent_fixed_magnitude`；公共锚 `F0` 满足 `F_ref,k · k_photo,k = F0`。**已作废写法**：「`F_ref` 为组内公共常数」（§9.49 定案 2 / §9.66 A `WEIGHT-FREF-PERFRAME-001` / §9.60 `FREF-BASELINE-001`）。单位以产物字段 `reference_flux_common_unit` 为准（现行 = `F_syn (Gaia XPSD absolute spectral integral)`；显式 `snr.reference_flux_adu` 覆盖时为 ADU）。⚠ `m_ref = 6.0` 是**参考电平的形式外推**（M42 Red 300 s 超饱和 1899×；HST M16 F657N 超 WFC3/UVIS 满井 1.9e4×），**不得**表述为「本帧能测到的 6 等星」 |
| `scalar_gate_rd` | —— | —— | 标量降级鲁棒离散门 |
| `scalar_gate_trend` | —— | —— | 标量降级系统趋势门 |
| `psfsw_enable` | true | —— | 是否生产 psfsw_robust 四分量 |
| `sparse_snr_layer` | true | —— | 是否产出稀疏帧内 SNR 层。**本期决议默认产出**（默认稀疏路径；负责人 2026-09-19 裁决） |
| `sparse_snr_spacing_px` | 64 | px | 稀疏层控制点间隔 Δ。**负责人 2026-09-19 定案 Δ=64px**（复用 Phase2 UPM 8×8/tile 控制网格；依据：实测 SNR 场相关长度 ℓ=48px ⇒ Δ/ℓ=1.33 近临界）。旧键 `sparse_snr_density`（单位 点/度²）保留登记但**不再承载本量**（守「一个字段只承载一个含义」） |
| `sparse_snr_density` | —— | 点/度² | **已退役（保留登记，不再承载任何量）**：原稀疏层控制点密度。负责人 2026-09-19 定案改用 `sparse_snr_spacing_px`（守「一个字段只承载一个含义」）；本行仅为登记册一一对应与「未定案」联锁缺口留痕，**禁止在生产配置中使用**。 |
| `snr_path` | `sparse_reconstruct` | —— | SNR 重建路径：`dense` / `sparse_reconstruct`（默认）/ `frame_reconstruct`；三条路径精度对比为论文核心实验（判据 SP-0） |

## 6. 接口/ABI

- entrypoint：信号+ivar+PSF+`a_k` → {source_snr, depth_m5, point_information, psfsw_robust, frame_snr[, sparse_snr_layer]}；
- 帧级 SNR（信噪比）经 drizzle 写入 HiPS 文件头；稀疏层作为标准层插入 HiPS；
- **Phase2/Phase3 不再产出 SNR**：Phase2 在叠加中现场消费本模块的单帧 SNR（换算逆方差权重），不输出 SNR 面；Phase3 无 SNR；
- 各类输出独立 schema，禁止混装。

## 7. 错误与边界

- 缺 `a_k`/PSF/方差 → fail-closed（信息权重不可凭空造）；
- 标量门失败 → 自动升级为空间模型（不得静默用标量）；
- `reference_flux` 未定义时 m5/SNR 不可输出；无 `photscale_fit` / `zero_point_valid=false` ⇒ 按显式回退 `group_median`（`reference_flux_scope` 落盘，**不伪造**），显式 `snr.reference_flux_adu` 优先；**逐帧中位数回退已删除（fail-closed），不得恢复**（§9.60）；
- **禁止**把组内 `F_ref` 相等当作门（组间硬闸门已删除，§9.66 A）；缺 `variance` 块 ⇒ `uncertainty_available=false`，**不得**声称已产出逐像素方差（§4.4）；
- 帧级 SNR（信噪比）无法计算（如缺真实信号参考）→ fail-closed，**不得用受天光影响的普通 SNR 代替**；
- 指定 `sparse_reconstruct` 路径而输入无稀疏层 → 按帧级执行并**显式记录实际路径**（不静默）；稀疏层损坏/不可重建 → fail-closed；
- `sparse_snr_density` 未定案 ⇒ 稀疏层不可生产（**联锁缺口**登记；不得编造数值）；
- 任何信号项未扣局部背景、被天光/背景抬高的 SNR → 判红（红线 §4.1）。

## 8. 测试与 Oracle

- 注入点源：理论 `σ_F=1/√W_psf` 与实测散度一致；
- 改变星表亮度分布不改变 `W_psf`、但改变 median source SNR（跨模块验证）；
- seeing/背景/透明度按理论改变信息权重；
- **加性背景平移不改变信号项**（注入恒定背景偏置，测光信号不变）；**天光散粒噪声增强时 σ_n 增大、帧级 SNR 按理论下降**（信噪比如实反映噪声，不允许被背景虚高）；**单调性负例**：固定源通量、天光 `B` 增大 ⇒ SNR 单调下降，`B→∞` 时 `SNR→0`；
- **注入-回收（C1）**：已知真值 `F_s` + 已知天光 + 已知噪声 ⇒ 回收 SNR = `F_s/σ_F`（三条路径各一组；不满足者判红）；
- 稀疏层：启用/不启用输出结构正确，稀疏层值可重建验证；**三路径精度对比（判据 SP-0）**：`dense` / `sparse_reconstruct` / `frame_reconstruct` 同输入重建稠密 SNR，报告精度差与存储量；无稀疏层而路径为 `sparse_reconstruct` 时实际路径须被显式记录（负例：静默降级判红）；
- 点源/面亮度口径分离：把面亮度 SNR 当帧级 SNR 使用必须判红。
- PSFSW 与 W_psf 分离性（PSFSW 驱动集成时 covariance 由实际组合系数传播）；
- **逐帧 `F_ref,k` 独立性负例**：人为要求组内 `F_ref` 相等（组间硬闸门）⇒ 必须判红——不同指向/不同光学系统的帧合法地有不同 `F_ref,k`；`w_k = SNR_k²/F_ref,k²` 的配对性**只在同一帧内**成立（§9.66 A `WEIGHT-FREF-PERFRAME-001`）；
- 1 worker vs N worker 一致。
