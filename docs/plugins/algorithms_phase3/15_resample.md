# 插件文档：resample（反向映射与重采样）

## 1. 职责与边界

- **职责**：对每个输出像素中心做 WCS inverse 定位输入 HEALPix，按产品语义采样核重采样，并传播不确定度/相关描述。
- **不是**：不做投影定义（projection）；不把普通 signal 插值规则机械套用于 point-source Q/W/PSF 参数；非有限/缺 tile 不以零填充。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §5.4（反向映射与采样）
- `docs/design/PHASE3_DETAILED_DESIGN.md` §3-§4
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（重采样方差传播）

## 3. 输入/输出数据合同

- **输入**：合同兼容 HiPS（signal、variance/correlation、coverage/validity、PSF 或 point-source statistics）、WCS 计划、采样核、配置。
- **输出**：平面重采样产品（信号/统计、variance、correlation、coverage、validity、effective PSF 或明确不支持点源）。
- 参考：`contracts/schemas/export_product.schema.json`。

## 4. 算法与公式要点

- 科学模式要求球面几何一致，**不允许平面距离替代**；
- 采样核是产品语义：
  - nearest：仅离散 mask/诊断或显式用户选择；
  - bilinear/高阶核：连续场，但必须说明通量/面亮度语义；
  - point-source Q/W/PSF 参数不得机械套用普通 signal 插值；
- 线性采样 `y=Rx`：`C_y = RC_xRᵀ`；仅输出对角 variance 必须给相关核/近似误差；
- coverage 不得代替 variance；
- 当前"四象限最近中心双线性"只能在 Oracle 与误差/边界定义后作为注册核，不能因现码存在就成永恒目标。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `sampler` | `bilinear` | —— | 重采样核（ALG 权威名 `sampler`，`docs/algorithms/PHASE3_RESAMPLE.md:8,:43` 取值 `nearest|bilinear`；旧字段名 `kernel` 与之分叉，按 DOC-SCI-001 A2 统一为 `sampler`） |
| `correlation_output` | true | —— | 是否输出相关核 |
| `order_limits` | —— | —— | 输入 order 选择约束（Nyquist） |

## 6. 接口/ABI

- entrypoint：HiPS+WCS 计划+核 → 平面重采样产品；
- 供 fits_output 写出。

## 7. 错误与边界

- 缺 tile/非有限 → validity 标记，不以零填充；
- point-source 模式缺 Q/W/PSF → fail-closed（不得插普通 signal 冒充）；
- 采样核语义未声明 → 拒绝。

## 8. 测试与 Oracle

- 常量面亮度、点源通量、variance/correlation 传播与注入源恢复；
- `C_y=RC_xRᵀ` 与高精度矩阵 oracle 对比；
- HEALPix/HiPS 外部实现交叉；跨 tile 连续场无缝；
- 不同 block/cache/worker 输出科学值一致；
- Q/W 重采样保持信息解释（注入源验证）。
