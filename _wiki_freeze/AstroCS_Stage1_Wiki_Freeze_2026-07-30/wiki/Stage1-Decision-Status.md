# Stage1 决策状态

> 本页面记录已冻结、待实验和明确不在本阶段的事项。

## 1. 已冻结事项

以下事项已由用户确认，Agent 可直接实现：

### 1.1 范围与流水线
- Stage1 固定流水线（单色 Light → 校准 → PlateSolve → PSF → Gaia 测光 → SNR → NSIDE → Drizzle → HISS）
- CLI/GUI 边界（CLI 不做分组、Master 制作、CFA、overscan、Stage2 调度）
- 单色输入
- 星点复用（只检测一次）

### 1.2 校准
- 三种校准模式（标准 (L-D)/F、曝光比例 [L-B-k_t(D-B)]/F、最优 Dark 系数 L-B=c+k(D-B)）
- 最优 Dark 失败自动回退曝光比例法
- Bias 不缩放，只减一次
- Flat 只做格式/结构校验，不重新归一化

### 1.3 Drizzle
- 自动 NSIDE（1~2 倍线性过采样）
- NESTED ordering
- 标准 pixfrac drop 语义（0 < pixfrac ≤ 1）
- float64 内部几何，float32 最终 signal
- 球面真实重叠面积
- 通量守恒（drop 未截断时 Σ = L_j）
- support = Σ a_jp / A_p，范围 0~1，存 uint8

### 1.4 HISS 容器
- XISF 式 Header + attachments
- 无 Footer、无 Checkpoint、无断点续写
- `.partial` 原子提交
- 自适应 Tile（d = min(9, log2(NSIDE/16))）
- FULL/BITMAP/SPARSE_LIST 占用编码
- 独立子块（occupancy/signal/support/SNR）
- 每子块独立 codec/transform/checksum
- RAW 必须可用

### 1.5 元数据
- 精简 FITS 风格
- 不保存完整 WCS/SIP
- 必需字段：NSIDE/ORDERING/RADESYS/TILENSID/PIXFRAC/PIXTYPE
- 测光字段：BUNIT=ASTROCS_RELATIVE_FLUX/PHOTSCAL/PHOTAPPL
- 校准字段：CALMODE/DARKREQ/DARKMODE/DARKSCL

### 1.6 SNR 控制点
- 每点仅 local_ipix(uint32) + snr(float32)
- 估计方法只在子块头保存一次

## 2. 待 C++ 实验事项

以下事项尚未冻结，Agent 只能做 C++ 实验并提交数据、分析和推荐，**不能替用户做最终决定**：

| 编号 | 事项 | 状态 |
|------|------|------|
| DQ-001 | signal 默认 codec/transform | 等待实验 |
| DQ-002 | support 默认 codec | 等待实验 |
| DQ-003 | BITMAP 默认 codec | 等待实验 |
| DQ-004 | SPARSE_LIST 编码与 codec | 等待实验 |
| DQ-005 | FULL/BITMAP/SPARSE 切换阈值 | 等待实验 |
| DQ-006 | checksum 算法 | 等待实验 |
| DQ-007 | 子块对齐 | 等待实验 |

实验结论只能写为：原始测量、候选排序、推荐方案、风险和适用范围。**不得把"推荐"直接改成最终冻结默认值。**

## 3. 明确不在本阶段

- Stage2 实现 or 修改
- 710 帧全量回归
- 用 Python 原型代替正式 C++ 实现
- HISS v1/v2 双路线维护
- 交付完整仓库或大型 ZIP
- 用户可见的双格式路线
