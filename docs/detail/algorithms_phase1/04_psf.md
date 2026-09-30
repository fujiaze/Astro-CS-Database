# 插件文档：psf（PSF 建模）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：估计空间变化的 PSF 模型 `P_k(u,v;x,y)` 及其参数、残差与适用域。
- **不是**：不做检测（star_detection）；不做测光；**PSF 拟合质量代理（FWHM、残差尺度等）只作诊断，不计入科学叠加权重**（最高设计 §3.1）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.6（硬约束：PSF 产品）与 §4.2（星表引导检测：候选星来自星表位置拟合）；数据对象见 `docs/detail/UNIFIED_MODEL.md` §1（观测模型）
- `docs/science/PSF_SIGNAL_WEIGHT.md`（PSF 与信息权重）
- `docs/detail/PHASE1_DETAILED_DESIGN.md` §6（PSF 模型）

## 3. 输入/输出数据合同

- **输入**：定标信号（拟合窗口像素值）、检测目录（候选星位置与拟合窗口；来自星表引导检测，检测定义域 = 星表位置，最高设计 §4.2）、初始参数、配置。逐像素 variance/ivar 加权拟合与 validity 掩膜为待实现项——现行 `moffat4_fit` 的输入面 = 图像 + 窗口几何 + 初值。
- **输出**：PSF 家族、参数、FWHM/椭率、有效域、拟合残差；空间变化模型及协方差（目标态，未落码——现实现仅帧级 Moffat4，见 §4）；`A_NEA = 1/ΣP²`（白噪声）；信息核 `PᵀC⁻¹P`。
- 产品至少提供 PSF 模型/地图 + 摘要。
- 参考：`docs/science/DATA_SEMANTICS.md` §15（DATA-P1-PSF：模块输入/输出数据合同正本）。

## 4. 算法与公式要点

- PSF 在每个位置归一为 `ΣP = 1`，包含像素响应；
- 空间变化模型：若通过均匀性门可降级为帧级 PSF；否则保留空间模型/控制点；**未落码（目标态）**——当前唯一实现路径 = Moffat4 帧级模型（`psf_spatial_order` 仅登记 0=帧级，非零分支无行为承载）；
- 点源信息权重依赖 PSF：`W_psf = a²PᵀC⁻¹P = 1/Var(F_hat)`（与 noise_snr、photometry 联动）；
- 有效域与拟合残差必须随产品输出；残差超阈标记 validity。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `psf_model` | `moffat4` | —— | PSF 母函数（当前实现：椭圆 Moffat4 7 参数；检测侧椭圆高斯归属 star_detection，见 `docs/science/STAR_DETECTION.md`） |
| `psf_spatial_order` | 0 | —— | 空间变化阶数（0=帧级） |
| `psf_uniformity_gate` | —— | —— | 均匀性门阈值 |
| `fit_residual_gate` | —— | —— | 拟合残差门 |

## 6. 接口/ABI

- entrypoint：信号 + 候选星窗口 + 初值 → PSF 模型/参数（帧级 Moffat4）；
- 输出模型的**生产消费者 = 零**：节点注册表**不声明** `artifact:p1_psf` 输入端口——`lib/infrastructure/scheduler/src/module_adapters.cpp` 逐字记「该边在注册表里不存在（`astrocs.phase1.star-psf` 的 `p1_psf` 端口"生产链路零消费者"）」，`docs/engineering/PIPELINE_BLOCK_CONTRACT.md` 亦把 `photometry ← p1_psf` 列作自测负例（幻边）。`p1_psf.json` 只经 `star_id` 关联作**本插件内部**的 PSF 域复核读数（`module_adapters.cpp` 落的 `p1_psf.json` 与其关联读）；noise_snr 的交付样本与深度按测光有效源独立构造，与 PSF 参数面解耦。

## 7. 错误与边界

- 候选星不足 → 明确失败或降级（记录），不静默给帧级 PSF 当空间模型；
- 残差超门 → validity 标记，按拟合质量不足登记；
- PSF 拟合质量代理只作诊断，不计入科学叠加权重（§1；最高设计 §3.1）。

## 8. 测试与 Oracle

- 注入已知 PSF 图像 → 参数恢复（FWHM/椭率/质心）符合精度；
- 空间变化模型在位置变化时的残差验证；
- `A_NEA` 与信息权重一致性 Oracle；
- PSF 不变量（ΣP=1、对称性按模型）。
