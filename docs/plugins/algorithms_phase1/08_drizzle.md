# 插件文档：drizzle（球面 Drizzle / HEALPix 累积）

## 1. 职责与边界

- **职责**：把定标后单帧图像按 WCS 重采样到球面 HEALPix/HiPS 格点，输出标准化单帧产品（HiPS + 结构化 JSON）。
- **不是**：不做测光/PSF 建模；**点源信息权重不能仅用 drizzle 后逐像素 ivar 重建而丢掉 PSF/协方差**；重采样相关噪声必须描述；**不产出外挂独立 SNR 文件**。

## 2. 权威依据

- 最高设计 §3.4（输出合同：帧级 SNR 入文件头、稀疏层插入）；数据对象见 `docs/design/UNIFIED_MODEL.md` §1（重采样线性算子）
- `docs/design/UNIFIED_MODEL.md`（数据对象表）
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（重采样方差传播）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §9

## 3. 输入/输出数据合同

- **输入**：定标信号、variance/ivar、validity、WCS、PSF 模型/地图、point_information、psfsw、depth_m5、frame_snr、[sparse_snr_layer]、配置。
- **输出**：
  - **HiPS 文件**：signal、pixel variance/ivar、support、coverage/validity、drizzle correlation/transfer 描述、PSF 模型、photometric response、point_information map、psfsw、depth、**帧级 SNR（信噪比）写入文件头**、**[稀疏帧内 SNR 层作为标准层]**、source catalog、manifest；
  - **结构化 JSON**：输出路径信息，符合 Phase2 输入格式。
- 参考：`contracts/schemas/hips_product.schema.json`、`contracts/schemas/manifest.schema.json`。

## 4. 算法与公式要点

源像素积分通量转面亮度，按球面交叠面积累积：

```text
B_j = x_j / A_pixel,j
S_p = Σ_j B_j a_jp / Σ_j a_jp
```

- 同时输出线性算子/足够方差传播信息、support、coverage、validity、相关噪声描述；
- 重采样是线性算子 `R`：`C_out = R C_in Rᵀ`；只存对角 variance 时必须另存 correlation kernel/scale 或可重建算子摘要；
- Drizzle 的 signal 单位、源/目标像素面积、pixfrac、归一必须统一；常量面亮度与总积分通量 Oracle 同时成立；
- pixfrac、像素面积、单位不可隐含。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `order` | —— | —— | HEALPix order（或 HiPS 尺度） |
| `pixfrac` | 1.0 | —— | drizzle 像素分数 |
| `pixel_scale` | —— | arcsec/px | 输出像素尺度（HiPS tile） |
| `nside` | —— | —— | HEALPix nside（与 order 等价） |
| `sparse_snr_layer` | false | —— | 是否将稀疏帧内 SNR 层插入 HiPS（来自 noise_snr） |

## 6. 接口/ABI

- entrypoint：图像组+WCS+科学层 → HiPS 产品目录 + 结构化 JSON；
- **帧级 SNR（信噪比）写入 HiPS 文件头**；稀疏层（启用时）作为标准层插入 HiPS；
- 输出原子目录，重开独立消费（Phase2 不需回读 raw light）。

## 7. 错误与边界

- WCS 缺失/不完整 → fail-closed；
- 相关噪声不存描述 → 不得宣称 variance 完备；
- 缺 tile/非有限 → validity 标记，不以零填充。

## 8. 测试与 Oracle

- 常量面亮度、积分通量、variance、correlation oracle 全过；
- `C_out = R C_in Rᵀ` 与高精度矩阵 oracle 对比；
- 不同 pixfrac/order 下科学值一致性；
- 产品从磁盘独立重开后足以执行 Phase2（不依赖进程内状态）；
- 1 worker vs N worker 一致。
