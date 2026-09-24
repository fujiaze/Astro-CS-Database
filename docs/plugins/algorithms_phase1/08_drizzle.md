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
  - **HiPS 文件**：signal、pixel variance/ivar、support、coverage/validity、drizzle correlation/transfer 描述、PSF 模型、photometric response、point_information map、psfsw、depth、**帧级 SNR（信噪比）写入文件头**、**[稀疏帧内 SNR 层（控制点存绝对 SNR）作为标准层]**、source catalog、manifest；
  - **结构化 JSON**：输出路径信息，符合 Phase2 输入格式；逐帧产品清单 `p1_products.json` 额外登记落盘形态（`storage_form`）、产品级索引路径（`index_path`）与指纹（`index_sha256` / `archive_sha256`），运行级登记 `coverage_index`（`path` / `sha256` / `n_frames` / `n_blocks`）；
  - **落盘形态**：由输入配置键 `storage_form` 选定——默认 `archive`（`<name>.hips.zst`，整包 tar + 逐成员 zstd 帧），可显式切 `bare`（`<name>.hips/`）；键缺失或留空 ⇒ 取默认并报 warn（不静默取默认）。两形态都写产品级索引，运行级写覆盖索引；归档内 `properties` 与裸形态逐字节一致。
- 参考：`eng/contracts/schemas/hips_product.schema.json`、`eng/contracts/schemas/manifest.schema.json`。

## 4. 算法与公式要点

源像素积分通量转面亮度，按球面交叠面积累积：

```text
B_j = x_j / A_pixel,j
S_p = Σ_j B_j a_jp / Σ_j a_jp
```

- 同时输出线性算子/足够方差传播信息、support、coverage、validity、相关噪声描述；
- 重采样是线性算子 `R`：`C_out = R C_in Rᵀ`；只存对角 variance 时必须另存 correlation kernel/scale 或可重建算子摘要；
- Drizzle 的 signal 单位、源/目标像素面积、pixfrac、归一必须统一；常量面亮度 Oracle（`x_j=B0·A_pixel,j`，`S_p=B0`，全 `pixfrac∈(0,1]`）与**严格**通量守恒（`Σ_p F_p=Σ_j x_j`，全 `pixfrac∈(0,1]`）同时成立——两者只在「核按 drop 面积归一 `w_jp=a_jp/A_drop,j` + 归一分母 `N_p=Σ_j w_jp·A_pixel,j`」这一组口径下相容；
- pixfrac、像素面积、单位不可隐含；
- **核按 `A_drop,j` 归一（canonical，`w_jp = a_jp / A_drop,j`）**：依据 F&H 2002 §7.2 式(7) 正下方定义（`a` 是 **the drop** 与输出像素的分数交叠 ⇒ `Σ_o a_io=1`）与 drizzlepac `cdrizzlebox.c` 的 `dover /= jaco`；这是唯一满足 `Σ_p F_p=Σ_j x_j` 的口径。
- **面亮度归一分母 `N_p = Σ_j w_jp·A_pixel,j`**：`S_p=F_p/N_p=Σ_j B_j a_jp/Σ_j a_jp`。若把分母换成覆盖面积 `D_p=Σ_j a_jp`，`pixfrac<1` 时 `S_p` 偏 `1/pixfrac²`（pf=0.8 → +56.25%，DISP-DRZ-009 负例判据）。
- 等价参数化（同一 `c_jp`）：`w'_jp=a_jp/A_pixel,j=pixfrac²·w_jp` 配分母 `Σ_j w'_jp=D_p/pixfrac²`；两种参数化给出**逐位相同**的 `S_p` 与 `variance_p`，但 `Phi_out` 只有 drop 面积归一口径等于 `Σ_j x_j`。
  **现状**：`processPixelSharedTiled` 的权重 = `overlap_area/drop_area`，并累加 `sumNorm = Σ_j w_jp·A_pixel,j`；`pixfrac==1` 时 `pixel_area ≡ drop_area` ⇒ `sumNorm ≡ sumArea` 逐位相同，默认路径产物逐字节不变。回归门 `p1drz_disp009`（常量面亮度 `|S_p/B0−1|<1e-3` 覆盖 `pixfrac∈(0,1]` + 「分母取覆盖面积必判红」负例控制）与 `drizzle_acceptance`（`Σ_p sumFlux=Σ_j x_j`，`pixfrac∈{0.1,0.5,0.8,1.0}` + `--inject-legacy-pixfrac2` 负例注入）。
  **适用域**：按 drop 面积归一与按输出像素面积归一的差异量是 `pixfrac²`，故 `pixfrac=1` 时两种口径给出同一结果；差异在 `pixfrac<1` 的配置下可观测。生产默认见 `eng/packaging/config/defaults.json`。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `order` | —— | —— | HEALPix order（或 HiPS 尺度） |
| `pixfrac` | 0.8 | —— | drop 收缩因子 ∈(0,1]；数值默认见本节注（唯一来源 = `defaults.json#drizzle.pixfrac`） |
| `pixel_scale` | —— | arcsec/px | 输出像素尺度（HiPS tile） |
| `nside` | —— | —— | HEALPix nside（与 order 等价） |
- `pixfrac` 的**数值默认唯一来源 = `eng/packaging/config/defaults.json` 的 `drizzle.pixfrac`**（本节不另立取值）；`provenance.flux_conservation_factor` **恒为 1**（drop 面积归一，与 `pixfrac` 无关；`docs/science/DRIZZLE.md` §7）。

| `sparse_snr_layer` | true | —— | 是否将稀疏帧内 SNR 层插入 HiPS（来自 noise_snr；控制点值 = 绝对通量型 SNR `F_ref/σ_F(x,y)`，与帧级同口径、同 `F_ref`）；**默认产出**（默认稀疏路径，`07_noise_snr.md` §4.2/§5） |

## 6. 接口/ABI

- entrypoint：图像组+WCS+科学层 → HiPS 产品目录 + 结构化 JSON；
- **帧级 SNR（信噪比）写入 HiPS 文件头**；稀疏层（启用时）作为标准层插入 HiPS，控制点存绝对 SNR（与帧级同口径、同 `F_ref`）；
- 输出原子目录，重开独立消费（Phase2 不需回读 raw light）。

## 7. 错误与边界

- WCS 缺失/不完整 → fail-closed；
- 相关噪声不存描述 → variance 完备性的宣称以该描述在盘为前提；
- 缺 tile/非有限 → validity 标记，不以零填充；
- **无覆盖/无数据 = NaN**（与支撑度 ≤0 一致），不用 0 或 ±Inf 冒充无效；NaN 采用**样本级掩膜**：被掩除的样本不参与该输出像素，剩余样本权重**重归一**；整个输出像素无有效覆盖则置 NaN（**覆盖级 NaN**）并**强制计数**（最高设计 §5.5/§10，规则见 `docs/science/DRIZZLE.md`）；
- **逐像素方差/ivar 产品面**：生产调度路径**已挂** `variance` 帧内命名块 —— `lib/infrastructure/scheduler/src/module_adapters.cpp`（`p1_op_drizzle`）按定案2 `NoiseWeightModelV1` blank-sky variance 经 `snr_noise_model_v1_fill` 填面后 `aio_frame_add_block(frame, "variance", AIO_BLOCK_FLOAT32, …)`；登记面 = `DATA-P1-DRZ` §11.1:295「variance 面（可选，帧内块）float32，ADU²」。引擎侧 `sumVarNum += v·w²`（`w = a_jp/A_pixel,j`），sink/writer finalize 出 V19 variance/ivar 子产品；`uncertainty_available` 为 provenance 判定结果（`true` ⇒ variance|ivar 位同时置位，`false` ⇒ 两位均不置位，禁占位子产品），**由磁盘事实给出，禁硬编码**。
  **显式降级（非静默，带 `var_status`/`var_reason`）**：noise model 退化（rc=1）⇒ `skipped_degenerate_empty_support`；填充面含非有限/非正值 ⇒ `skipped_fill_failed`（全零方差面会让引擎整像素 `varianceValue<=0 ⇒ continue`，抹掉 signal/support，故 fail-closed）。凡「逐像素方差已由生产路径产出」的主张**必须**附 `n_variance_tiles>0` 的磁盘证据；旧表述「生产调度路径不挂 `variance` 块 ⇒ `has_variance=0` ⇒ `uncertainty_available=false`（原因 `ivar_product_missing_frame_snr_fallback`）」与工作区现状**不符，已作废**；双实现分裂见 `07_noise_snr.md` §4.4。

## 8. 测试与 Oracle

- 常量面亮度、积分通量、variance、correlation oracle 全过；
- `C_out = R C_in Rᵀ` 与高精度矩阵 oracle 对比；
- 不同 pixfrac/order 下科学值一致性；
- 产品从磁盘独立重开后足以执行 Phase2（不依赖进程内状态）；
- 1 worker vs N worker 一致。
