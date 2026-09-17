# 插件文档：noise_snr（噪声/SNR/信息权重）

## 1. 职责与边界

- **职责**：从校准方差、背景、PSF 与光度响应估计逐像素噪声、逐源 SNR、深度 `m5`、点源信息权重 `W_psf` 与**帧级 SNR（信噪比）**（写入 HiPS 文件头的唯一帧级参考）。
- **不是**：不生产"一个模糊的 snr 字段"；不把 median source SNR 当科学权重；不把 PSFSW 复合权重冒充 Fisher information；**不产出外挂独立 SNR 文件**（帧级 SNR（信噪比）写入 HiPS 文件头）。

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
  - **`frame_snr`**：帧级 SNR（信噪比），**写入 HiPS 文件头**；
  - **`sparse_snr_layer`**（可选，`sparse_snr_layer=true` 时）：帧内稀疏控制点 SNR 层，作为标准层插入 HiPS 文件内。
- 参考：`contracts/schemas/noise_snr_output.schema.json`。

## 4. 算法与公式要点

```text
W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k = 1/Var(F_hat_k)
白噪声: W_psf,k = a_k² Σ_p P_k,p² / σ_pix,k² = a_k² / (σ_pix,k² A_NEA,k)
SNR_k²(F_ref) = F_ref² W_psf,k
```

### 4.1 帧级 SNR（信噪比）（frame_snr）

- 是**唯一帧级参考**，写入 HiPS 文件头；
- **是未加权的原始信噪比，不是权重**——它只描述"这一帧的真实源信号相对真实噪声有多强"这一客观测量事实，不包含任何为某次叠加服务的加权；权重由 Phase2 逆方差叠加时从 SNR 现场计算（见最高设计 §4.3）；
- **可靠且独立**：必须是**真实的信号与噪声比例**，不受天光影响——普通 SNR（全局信号方差/噪声方差，含天光背景）随天光变亮而虚高，不能作唯一帧级参考；
- 不随天光/透明度正常波动而漂移（天光变化只改变独立的背景项，不改变"信号/噪声"定义）。

**与 PixInsight 公开方法学的关系（精确对标）**：PixInsight 核心闭源，但其《New Image Weighting Algorithms》参考文档公开了完整方法学，其中是**两个不同的量**，必须区分：

| 量 | 定义（官方） | 性质 | AstroCS 对应 |
|---|---|---|---|
| **PSFSNR** | ratio-of-powers 信噪比：`c3·√(Σ_j f_j²) / (c4·σ_n²)`，f_j 为各星 PSF 通量（FWTM 孔径内像素减局部背景求和），σ_n 为稳健噪声（MRS/N*）；c3、c4 是其模拟数据标定常数 | **未加权的原始信噪比** | **frame_snr 对标此量的方法学**（独立标定常数，不照抄 c3/c4） |
| **PSF Signal Weight（PSFSW）** | 综合**图像质量权重**：信号总量 × 信号集中度（mean flux，随 FWHM 变小而增大）/（稳健噪声 × 稳健平均背景 M*） | **权重**，额外含分辨率/FWHM 与背景梯度惩罚，不是信噪比 | 仅显式 `weight_mode=psfsw_robust` 时使用，四分量独立存储 |
| 标准 SNR | `σ²/σ_n²`，全局尺度估计 | 信噪比，但**受天光/梯度正向影响**，官方明确指出它会给目标 SNR 很低的亮背景帧虚高权重 | 不采用 |

- 共同的、我们借鉴的方法学：① 信号只从检测到的恒星经 PSF/孔径混合测光得到（不用拟合振幅，只用采样像素减独立估计的局部背景）；② 噪声用稳健多尺度估计；③ 背景（天光）作为**独立的稳健分量**估计与扣除，不进入信号——这三点保证 frame_snr 不受天光影响；
- PixInsight 官方同样**不把权重存进图像**：校准阶段只把信号/噪声/背景分量写入元数据（FITS 关键字 PSFFLX/PSFMFL/PSFMST/PSFNST/NOISE 等），权重在 ImageIntegration 集成时才计算——与 AstroCS"数据库存原始 SNR、Phase2 消费时才算权重"的设计一致；
- PSFSW 归一化常数（c1/c2）与 PSFSNR 常数（c3=1.350×10⁻⁷、c4=4.987×10⁺⁶）均由 PixInsight 自造 1000 张 4096² 模拟图标定，**AstroCS 不照抄**：采用无量纲/物理量纲定义，常数由本项目合成数据（验收 L1）独立标定并冻结。

**数学定义与换算**（公共参考通量 `F_ref`，见 `docs/science/PSF_SIGNAL_WEIGHT.md`）：

```text
# 帧级 SNR（未加权原始信噪比，写入文件头）：真实源信号 / 真实噪声
SNR_k(F_ref) = F_ref · sqrt(W_psf,k) = F_ref / σ_F,k
W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k  （点源信息，σ_F,k² = 1/W_psf,k；信噪比本身未做任何加权）

# Phase2 叠加时现场换算为逆方差权重（UPM 已归一到公共通量尺度）：
w_k = 1/σ_F,k² = SNR_k(F_ref)² / F_ref²   ⇒  F_ref 为组内公共常数，w_k ∝ SNR_k²
```

- HiPS 是数据库：帧产品长期保存、可被任意多次、任意科学目标的叠加消费，因此入库的是客观的未加权 SNR（与具体集成无关的观测量），把"选哪种权重模式"留给 Phase2；
- Phase2 默认 point_information 逆方差叠加；`weight_mode=psfsw_robust` 时使用 `psfsw_robust` 复合权重（信号/集中度/稳健噪声/稳健背景四分量独立存储，模式显式选择，不自动切换）；
- 稀疏帧内层启用时，每个控制点同样存未加权 SNR(x,y) 而非权重。

### 4.2 稀疏帧内 SNR 层（可选）

- `sparse_snr_layer=true` 时：生成稀疏控制点 SNR 层，作为**标准层插入 HiPS 文件内**，用于帧内精细 SNR 参考；
- 实际 SNR = **帧级 × 帧内**（SNR 是信噪比，不是权重）；
- 不启用 → 只输出帧级；启用 → 帧级 + 稀疏帧内；
- 稀疏层的位置/值/采样覆盖写入 manifest。

### 4.3 其他要点

- 白噪声时为简单形式；相关噪声时用完整信息核；
- 标量降级门：仅当帧内 `W_psf(x,y)` 鲁棒相对离散与系统趋势低于阈值才存帧级标量；否则存 map/控制点/多项式/HEALPix，摘要带 p05/p50/p95、最大系统偏差、覆盖、模型误差；
- PSFSW 是综合图像质量**权重**（含 FWHM/背景梯度），无量纲、组内相对、可驱动显式 `weight_mode=psfsw_robust` 集成，**不是信噪比、不是 ivar**；frame_snr 对标的是 PSFSNR（未加权原始信噪比）而非 PSFSW。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `reference_flux` | —— | e⁻/s | 固定参考通量（m5/SNR 定义必需） |
| `scalar_gate_rd` | —— | —— | 标量降级鲁棒离散门 |
| `scalar_gate_trend` | —— | —— | 标量降级系统趋势门 |
| `psfsw_enable` | true | —— | 是否生产 psfsw_robust 四分量 |
| `sparse_snr_layer` | false | —— | 是否启用稀疏帧内 SNR 层 |
| `sparse_snr_density` | —— | 点/度² | 稀疏层控制点密度（启用时） |

## 6. 接口/ABI

- entrypoint：信号+ivar+PSF+`a_k` → {source_snr, depth_m5, point_information, psfsw_robust, frame_snr[, sparse_snr_layer]}；
- 帧级 SNR（信噪比）经 drizzle 写入 HiPS 文件头；稀疏层作为标准层插入 HiPS；
- 各类输出独立 schema，禁止混装。

## 7. 错误与边界

- 缺 `a_k`/PSF/方差 → fail-closed（信息权重不可凭空造）；
- 标量门失败 → 自动升级为空间模型（不得静默用标量）；
- reference_flux 未定义时 m5/SNR 不可输出；
- 帧级 SNR（信噪比）无法计算（如缺真实信号参考）→ fail-closed，**不得用受天光影响的普通 SNR 代替**。

## 8. 测试与 Oracle

- 注入点源：理论 `σ_F=1/√W_psf` 与实测散度一致；
- 改变星表亮度分布不改变 `W_psf`、但改变 median source SNR（跨模块验证）；
- seeing/背景/透明度按理论改变信息权重；
- **天光变化不改变帧级 SNR（信噪比）**（独立于天光的不变量测试）；
- 稀疏层：启用/不启用输出结构正确，稀疏层值可重建验证；
- PSFSW 与 W_psf 分离性（PSFSW 驱动集成时 covariance 由实际组合系数传播）；
- 1 worker vs N worker 一致。
