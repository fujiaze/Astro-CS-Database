# 插件文档：star_detection（源探测）

## 1. 职责与边界

- **职责**：检测图像中的源（星点/延展源），输出位置、质心/矩、源身份与 selection function 参数。
- **不是**：不是图像灵敏度本身；不做 PSF 建模（psf 模块）；不做测光（photometry 模块）；检测目录是下游输入，不是产品权重。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §3.6（硬约束，**实测行范围 `:319-344`**；星表引导检测条 `:323-325`）与 §3.2（星表引导检测范式，`:119-120`）；硬约束转引 `docs/plugins/algorithms_phase1/**` 与 `docs/science/**`。（**2026-09-20 订正**：原文行锚「§3.6:195」**悬空**——该行已不存在；依据 `ENGINEERING_SPEC.md:129`「锚存活」；检测阈值的冻结定义见 `docs/science/STAR_DETECTION.md:18-19`、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:36`、`docs/algorithms/GATES_AND_TOLERANCES.md:38-39`）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §5（背景、有效性与源检测）
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（质心/矩不确定度）

## 3. 输入/输出数据合同

- **输入**：定标+cosmetic 后信号、variance/ivar、validity、背景模型（若已有）、配置。
- **输出**：source catalog（源 ID、像素坐标、天球坐标、质心/矩、局部 SNR、flags）+ selection function/completeness 参数。
- 检测、PSF、WCS、测光、SNR 的 source row 绑定同一 `frame_id/source_id`。
- 参考：`contracts/schemas/source_catalog.schema.json`。

## 4. 算法与公式要点

- **权威检测范式 = 星表引导拟合**（最高设计 §3.2 `ASTROCS_DESIGN.md:119-120`）：检测定义域是**星表位置**（用本帧 WCS 把 Gaia 星表反向投影到像素域），只对星表位置做质心/PSF 拟合；拟合成功即星点，失败**直接丢弃**；**全图盲检测连通域路径不是权威路径**（§3.6 `:325`；§9.49 定案 1，2026-09-20）。
- 检测阈值 = `median(img) + 5.0·bgnoise`（**全局背景噪声 RMS 的倍数**，`bgnoise` 由 FnNoise1 行差分族估计；阈值作用于 σ=2 平滑图；实现 `sdet_api.cpp:1782-1792`）——**仅适用于「第一轮盲解」**（全图盲检测 → 粗匹配 → 初解 WCS，只为星表投影提供近似指向，该轮星表**不是**权威科学产品；实现即既有 `sdet_api.cpp`，最高设计 §3.2 `:120`）。**第二轮精解**用星表引导检测后的高纯度星表重解 WCS，此结果才是权威 WCS。检测路径不消费逐像素 variance/ivar。局部噪声自适应为目标态、当前未实现（GAP 登记），文档按现状描述；
- 质心/矩与不确定度：一阶矩质心、二阶矩，误差来自局部噪声传播；
- 输出 selection function（完备性 vs 亮度/位置）和 completeness 参数；
- 检测统计量与下游 PSF/测光解耦：检测目录不直接成为科学权重。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `detection_threshold` | 5.0 | σ（全局 bgnoise） | **第一轮盲解专用 / 显式声明的可选诊断**（**不是**模块主路径配置——主路径 = 星表引导拟合，不消费此键）；语义 = `median(img)+5.0·bgnoise`（σ=2 平滑图上判定），与 `config/defaults.json#detection.threshold_sigma` 同义（**2026-09-20 订正**，依据 §9.49 定案 1 + `ASTROCS_DESIGN.md:119,260,262`） |
| `min_area` | 2 | px | 最小连通像素数。**仅第一轮盲解 / 连通域诊断**（星表引导路径不做连通域，2026-09-20 订正） |
| `deblend` | true | —— | 是否解混。**仅第一轮盲解 / 显式声明的可选诊断**（星表引导路径按星表位置逐源拟合，不做盲解混，2026-09-20 订正） |
| `selection_function` | true | —— | 是否输出 selection function |

## 6. 接口/ABI

- entrypoint：图像+ivar+validity → source catalog；
- 单源行结构版本化，绑定 frame_id/source_id。

## 7. 错误与边界

- 输入全 NaN/全饱和 → 拒绝并记录，不产出空目录冒充成功；
- 边界源标记边界 flag；
- 亮星饱和/拖线标记，不参与后续 PSF/测光默认路径。

## 8. 测试与 Oracle

- 合成图像注入已知源（位置/亮度分布已知）→ 检测率、误检率、质心精度符合理论；
- selection function 与注入分布一致；
- 改变星表亮度分布只改变 source-SNR 摘要，不改变信息权重（跨模块验证）；
- 1 worker vs N worker 一致。
