# 插件文档：drizzle（球面 Drizzle / HEALPix 累积）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：把定标后单帧图像按 WCS 重采样到球面 HEALPix/HiPS 格点，输出标准化单帧产品（HiPS + 结构化 JSON）。
- **不是**：不做测光/PSF 建模；**点源信息权重不能仅用 drizzle 后逐像素 ivar 重建而丢掉 PSF/协方差**；重采样相关噪声必须描述；**不产出外挂独立 SNR 文件**。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.4（输出合同：帧级 SNR 入文件头、稀疏层插入）与 §2.2（创新点二：跨帧可用的绝对信噪比）；数据对象见 `docs/design/UNIFIED_MODEL.md` §1（重采样线性算子）
- `docs/design/UNIFIED_MODEL.md`（数据对象表）
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（重采样方差传播）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §9

## 3. 输入/输出数据合同

- **输入**：定标信号、variance/ivar、validity、WCS、PSF 模型/地图、point_information、psfsw、depth_m5、frame_snr、[sparse_snr_layer]、配置。
- **输出**：
  - **HiPS 文件**：signal、pixel variance/ivar、support、coverage/validity、drizzle correlation/transfer 描述、PSF 模型、photometric response、point_information map、psfsw、depth、**帧级 SNR（信噪比）写入文件头**、**[稀疏帧内 SNR 层作为标准层]**、source catalog、manifest；
  - **结构化 JSON**：输出路径信息，符合 Phase2 输入格式。
- 参考：`eng/contracts/schemas/hips_product.schema.json`、`eng/contracts/schemas/manifest.schema.json`。

## 4. 算法与公式要点

源像素积分通量转面亮度，按球面交叠面积累积：

```text
B_j = x_j / A_pixel,j
S_p = Σ_j B_j a_jp / Σ_j a_jp
```

- 同时输出线性算子/足够方差传播信息、support、coverage、validity、相关噪声描述；
- 重采样是线性算子 `R`：`C_out = R C_in Rᵀ`；只存对角 variance 时必须另存 correlation kernel/scale 或可重建算子摘要；
- Drizzle 的 signal 单位、源/目标像素面积、pixfrac、归一必须统一；常量面亮度 Oracle（`x_j=B0·A_pixel,j`，`S_p=B0`，全 `pixfrac∈(0,1]`）与条件通量守恒（`Σ_p F_p=pixfrac²·Σ_j x_j`）同时成立；
- pixfrac、像素面积、单位不可隐含；
- **归一必须走 `sb_a_pixel` 路径**：`sb_weight = a_jp / A_pixel`（面亮度 / 像素面积）；`w = a_jp / A_drop,j`（`drizzle_engine.cpp` 的 `A_drop` 归一）在 `pixfrac<1` 时偏 `1/pixfrac²`，**禁用于绝对面亮度**，见 `docs/science/DRIZZLE.md` §5/§7 与 `DISP-DRZ-009`。
- 依据：`pixfrac=0.8` 实测 `A_drop` 路径面亮度 **×1.5625（+56.25%）**、方差 **×2.4414**，与 `1/pf²`、`1/pf⁴` 逐位吻合；`sb_a_pixel` 正确实现已存在（`v6_drizzle_science.cpp`：`pixfrac<1` 时对 `A_drop` 归一**显式 fail-closed**），但生产 `hp_drizzle_run` 仍硬编码 `A_drop` 路径 ⇒ **须改走 `sb_a_pixel`**。
  **边界（诚实）**：L4 全部 12 配置 `pixfrac=1.0` ⇒ **本次数据偏差为 0**，属**潜在隐患**，**不得**写成「本次数据已被污染」。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `order` | —— | —— | HEALPix order（或 HiPS 尺度） |
| `pixfrac` | 1.0 | —— | drop 收缩因子 ∈(0,1]；默认 1.0 = 严格通量守恒端点（`docs/science/DRIZZLE.md:95-98`；pixfrac<1 须记 `provenance.flux_conservation_factor=pixfrac²`） |
| `pixel_scale` | —— | arcsec/px | 输出像素尺度（HiPS tile） |
| `nside` | —— | —— | HEALPix nside（与 order 等价） |
| `sparse_snr_layer` | true | —— | 是否将稀疏帧内 SNR 层插入 HiPS（来自 noise_snr）；**默认产出**（默认稀疏路径，`07_noise_snr.md` §4.2/§5） |

## 6. 接口/ABI

- entrypoint：图像组+WCS+科学层 → HiPS 产品目录 + 结构化 JSON；
- **帧级 SNR（信噪比）写入 HiPS 文件头**；稀疏层（启用时）作为标准层插入 HiPS；
- 输出原子目录，重开独立消费（Phase2 不需回读 raw light）。

## 7. 错误与边界

- WCS 缺失/不完整 → fail-closed；
- 相关噪声不存描述 → 不得宣称 variance 完备；
- 缺 tile/非有限 → validity 标记，不以零填充；
- **无覆盖/无数据 = NaN**（与支撑度 ≤0 一致），不用 0 或 ±Inf 冒充无效；NaN 采用**样本级掩膜**：被掩除的样本不参与该输出像素，剩余样本权重**重归一**；整个输出像素无有效覆盖则置 NaN（**覆盖级 NaN**）并**强制计数**（最高设计 §5.5/§10，规则见 `docs/science/DRIZZLE.md`）；
- **逐像素方差/ivar 产品面当前不可得（登记，未闭合）**：生产调度路径不挂 `variance` 块 ⇒ `has_variance=0` ⇒ `uncertainty_available=false`（原因 `ivar_product_missing_frame_snr_fallback`）；双实现分裂见 `07_noise_snr.md` §4.4。凡「逐像素方差已由生产路径产出」的主张**不得**提出。

## 8. 测试与 Oracle

- 常量面亮度、积分通量、variance、correlation oracle 全过；
- `C_out = R C_in Rᵀ` 与高精度矩阵 oracle 对比；
- 不同 pixfrac/order 下科学值一致性；
- 产品从磁盘独立重开后足以执行 Phase2（不依赖进程内状态）；
- 1 worker vs N worker 一致。
