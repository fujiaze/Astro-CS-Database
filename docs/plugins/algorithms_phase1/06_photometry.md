# 插件文档：photometry（测光/通量定标）

## 1. 职责与边界

- **职责**：对检测源做孔径/PSF 测光，把本帧 signal 映射到统一线性通量尺度 `d = a_k F P + n`，给出 `a_k` 及其不确定度。
- **不是**：不做绝对光度定标到物理流量；不做 Phase2 集成（integration）；相对标度不足时标记不可跨帧合并，**不用 median stellar flux 静默代替**。

## 2. 权威依据

- 最高设计 §3.6（硬约束：测光）；数据对象见 `docs/design/UNIFIED_MODEL.md` §1（观测模型）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §7（测光）
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（通量方差）

## 3. 输入/输出数据合同

- **输入**：定标信号、variance/ivar、validity、PSF 模型、WCS、检测目录、配置。
- **输出**：源通量 `F`、通量方差 `Var(F)`、`a_k`（光度响应）及其不确定度、颜色项、有效域、测光 flags。
- 参考：`contracts/schemas/photometry_output.schema.json`。

## 4. 算法与公式要点

- 孔径测光：孔径定义、sky 环、误差传播；
- PSF 测光：在 PSF 模型下最大似然通量 `F_hat = Q/W`，`Var(F_hat)=1/W`（与噪声/信息权重联动）；
- 光度响应 `a_k`：把本帧 ADU/e⁻ 映射到统一线性通量尺度；颜色项与适用域声明；
- 相对标度不足 → 标记"不可跨帧合并"，不静默降级。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `mode` | `psf` | —— | psf/aperture |
| `aperture_radius` | 2×FWHM | px | 孔径测光半径 |
| `sky_annulus` | —— | px | sky 环 |

（绝对通量锚定由 Gaia XP 合成通量 `F_syn` 与**输出** `location`/`scale` 承担，`docs/science/PHOTOMETRY.md:7,:18-19,:59`；`zero_point` 作为配置输入字段已于 P5-SNR 订正删除，`docs/algorithms/PHOTOMETRIC_FIT.md:9`，不得再登记为旋钮。）

## 6. 接口/ABI

- entrypoint：信号+ivar+PSF+WCS+目录 → 通量表+`a_k`；
- `a_k` 随产品输出，供 Phase2 使用。

## 7. 错误与边界

- 源太暗/太亮 → 测光 flags，不产出无意义通量；
- `a_k` 不确定度缺失 → 标记不可跨帧合并；
- 饱和/拖线源标记，不进默认路径。

## 8. 测试与 Oracle

- 注入已知通量源 → 通量恢复 bias/variance 符合理论；
- PSF 测光与独立孔径测光交叉；
- `σ_F=1/√W_psf` 理论一致性；
- `a_k` 不确定度传播到 Phase2 covariance 验证。
