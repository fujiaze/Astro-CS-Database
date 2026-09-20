# 插件文档：photometry（测光/通量定标）

## 1. 职责与边界

- **职责**：对检测源做孔径/PSF 测光，把本帧 signal 映射到统一线性通量尺度 `d = a_k F P + n`，给出 `a_k` 及其不确定度。
- **不是**：不做绝对光度定标到物理流量；不做 Phase2 集成（integration）；相对标度不足时标记不可跨帧合并，**不用 median stellar flux 静默代替**；**不以物理单位论证标定因子**——`a_k`/`k_photo` 的绝对值无物理意义，判据 = **测光一致性**（「帧间一致性」是语义目标与报告字段，**不是门禁**；负责人 §9.49 定案 2，变更 claim `PHOT-GATE-DROP-001`）（`docs/science/PHOTOMETRY.md` §1）。

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

### 4.1 测光一致性判据（**待从误差预算推导**；`0.03 mag` 已作废）

- 判据只有**一条**（尺度无关）：**测光一致性**（施加后星点星等与 Gaia 残差的散度/MAD 小）；「帧间一致性」是**语义目标与报告字段，不是门禁**（§9.49 定案 2；变更 claim `PHOT-GATE-DROP-001`）；门只有一个 = **单帧标定是否可信**，与其它帧无关；
- ⚠ **`≤0.03 mag` 阈值已作废**（§9.67 定案 6，2026-09-20，负责人原话「我们门禁设置是需要科学推导。你这个门禁显然是瞎编的」）。该阈值在 M42 拥挤场 + 2.0–2.6 px seeing + 16bit 下两种口径都达不到（修复后中位 `0.0617` / 最坏 `0.1160 mag`；§9.54 待裁决项）；
- **`MAD ≤ 0.03 等` 不是硬门**（§9.50 定案 3）：判据 = **测光正确 + 拟合收敛**；
- 新判据**必须**由**误差预算逐项推导**（光子噪声 + PSF 拟合不确定度 + 平场/天光残余 + 星等定标误差的合成），给出**逐项数值 + 出处**，写成可复核文档后走**变更 claim**；
- 推导完成前，本模块**不得**声称存在通过/不通过的测光一致性门；`0.03 mag` **不得**在任何实现/测试/文档中复活（无判据 ⇒ 只报诊断量，不得假装有门）。

### 4.2 外部参考实现的引用纪律（PMM / PhotometricMosaic）

- PhotometricMosaic（PMM）**本体禁止再分发、禁止修改**（§9.51 R51-4）：① **可**读源码做方法研究；② **不得复制代码**；③ **不得当引用文献**；④ 不得再分发；
- **A6 测光口径不照搬 PMM**（§9.51 R51-3）：PMM 用的是「孔径口径」，正是 A6 定案要淘汰的口径；
- 可借鉴的是其**报告范式**：跨帧一致性只作 **warning / 报告字段**，不作拒绝帧的门（§9.51 R51-2；实现字段 `photscale_spread_dex` / `photscale_spread_warn` / `photscale_spread_gate = "none (owner ruling 9.49: frame-independent)"`，§9.62）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `mode` | `psf` | —— | psf/aperture |
| `aperture_radius` | 2×FWHM | px | 孔径测光半径 |
| `sky_annulus` | —— | px | sky 环 |

绝对通量锚定由 Gaia XP 合成通量 `F_syn` 与输出 `location`/`scale` 承担（见 `docs/science/PHOTOMETRY.md`）；配置中无 `zero_point` 字段。

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
- `a_k` 不确定度传播到 Phase2 covariance 验证；
- **测光一致性判据（§4.1）落地后须有能红能绿的负例**；推导完成前不得设置通过/不通过门，也不得复活 `0.03 mag`；
- **组间一致性不得作门**：人为加入跨帧 k/scale 一致性门 ⇒ 必须判红（`PHOT-GATE-DROP-001`）；
