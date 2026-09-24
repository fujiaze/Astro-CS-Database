# 插件文档：cosmetic（坏点/宇宙线修正）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：生成/应用坏点、热像素、宇宙线与异常像素的修正与 validity 标志。
- **不是**：不做背景估计；不做排异（排异是 Phase2 的推断）；修正必须可追溯、可回退。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.6（硬约束：cosmetic/validity）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §5（有效性域）
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（修正对协方差的影响）

## 3. 输入/输出数据合同

- **输入**：定标信号 `y`、cosmetic map、饱和/非线性状态、坏点列表（可含注入）。
- **输出**：修正后信号（或标记）、validity map（坏点/饱和/cosmetic/边界/插值）、修正记录（哪些像素被改、用什么）。
- 参考：`eng/contracts/schemas/cosmetic_output.schema.json`。

## 4. 算法与公式要点

- 坏点/热像素：由 master cosmetic map 或暗场统计判定；修正值来源（中值/插值/相邻）必须记录；
- 宇宙线：基于局部统计的异常检测（阈值来自预测噪声；固定全局阈值属另一口径）；
- validity 类型：NaN/Inf、坏点、饱和、cosmetic、边界、插值、星轨/严重形变；
- 修正写入后，对应像素的 variance 必须同步更新（插值处方差按插值权重传播，原方差随之被替换）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `cosmetic_map_path` | —— | —— | master cosmetic map |
| `saturation_level` | —— | ADU/e⁻ | 饱和阈值（元数据或显式） |
| `cr_detection` | true | —— | 宇宙线检测开关 |
| `cr_sigma` | 5.0 | σ | 宇宙线检测阈值（预测噪声倍数） |
| `interpolation` | `neighbor` | —— | 修正插值方式 |

## 6. 接口/ABI

- entrypoint：定标信号 + cosmetic map → 修正信号 + validity + 修正记录；
- 所有权按 C ABI；修正记录写入 manifest。

## 7. 错误与边界

- 饱和区不参与宇宙线判定；
- 修正后必须能区分"原始"与"修正"（保留原始备份或修正标记），**修正一律留可辨认痕迹**；
- 极端情形（大片坏区）一律显式登记：零值填充只作具名降级。

## 8. 测试与 Oracle

- 注入已知坏点/宇宙线，验证检测率与修正正确性；
- 方差更新 Oracle（插值处方差符合理论）；
- 修正可回退性测试；
- validity 传播到下游产品测试。
