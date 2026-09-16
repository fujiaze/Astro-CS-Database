# 插件文档：noise_snr（噪声/SNR/信息权重）

## 1. 职责与边界

- **职责**：从校准方差、背景、PSF 与光度响应估计逐像素噪声、逐源 SNR、深度 `m5`、点源信息权重 `W_psf` 与**帧级 SNR（信噪比）**（写入 HiPS 文件头的唯一帧级参考）。
- **不是**：不生产"一个模糊的 snr 字段"；不把 median source SNR 当科学权重；不把 PSFSW 复合权重冒充 Fisher information；**不产出外挂独立 SNR 文件**（帧级 SNR（信噪比）写入 HiPS 文件头）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §3.3（输入合同）、§3.4（输出合同：帧级 SNR（信噪比）入文件头）
- `docs/design/UNIFIED_MODEL.md`（数据对象表：frame_snr、sparse_snr_layer）
- `docs/science/NOISE_MODEL.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`
- `docs/design/PHASE1_DETAILED_DESIGN.md` §8（SNR、PSF Signal Weight）

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
- **是信噪比，不是权重**——权重由 Phase2 逆方差叠加从 SNR 计算（见最高设计 §4.3）；
- **可靠且独立**：必须是**真实的信号与噪声比例**，不受天光影响——普通 SNR（signal/σ，含天光背景）受天光影响，不能作唯一帧级参考；
- 语义类似 PSF-SNR（信噪比）：基于真实信号（源/PSF 通量）与真实噪声（传播后的 variance），而不是背景之上的原始计数比；
- 不随天光/透明度正常波动而漂移（天光变化只改变背景项，不改变"信号/噪声"定义参考）。

### 4.2 稀疏帧内 SNR 层（可选）

- `sparse_snr_layer=true` 时：生成稀疏控制点 SNR 层，作为**标准层插入 HiPS 文件内**，用于帧内精细 SNR 参考；
- 实际 SNR = **帧级 × 帧内**（SNR 是信噪比，不是权重）；
- 不启用 → 只输出帧级；启用 → 帧级 + 稀疏帧内；
- 稀疏层的位置/值/采样覆盖写入 manifest。

### 4.3 其他要点

- 白噪声时为简单形式；相关噪声时用完整信息核；
- 标量降级门：仅当帧内 `W_psf(x,y)` 鲁棒相对离散与系统趋势低于阈值才存帧级标量；否则存 map/控制点/多项式/HEALPix，摘要带 p05/p50/p95、最大系统偏差、覆盖、模型误差；
- PSFSW 是无量纲、组内相对、可驱动显式 `weight_mode=psfsw_robust` 集成，**不是 ivar**。

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
