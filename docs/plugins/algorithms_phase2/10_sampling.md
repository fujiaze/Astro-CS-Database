# 插件文档：sampling（控制采样）

## 1. 职责与边界

- **职责**：为 UPM 拟合选取控制点，避开源、饱和、坏点和高结构区域，保证模型可辨识。
- **不是**：不做模型拟合（upm）；采样不是科学权重；不做排异推断（rejection 可辅助但独立）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.2（控制采样）
- `docs/design/PHASE2_DETAILED_DESIGN.md` §4（UPM 控制点要求）

## 3. 输入/输出数据合同

- **输入**：Phase1 产品组、coverage、validity、检测目录（避开亮星）、配置。
- **输出**：控制点集合（坐标、值、噪声、mask）、覆盖与统计。
- 参考：`contracts/schemas/control_points.schema.json`。

## 4. 算法与公式要点

- 控制点避开：源（检测目录）、饱和、坏点、高结构区域；
- 采样策略：空间均匀 + 分层（按背景/噪声），保证 UPM 可辨识；
- 记录每个控制点的 signal、variance、validity、是否参与拟合；
- 数量下限由 UPM 参数自由度决定，不足 → 明确失败。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `spacing` | —— | px/deg | 空间采样间隔 |
| `bright_star_mask` | —— | —— | 亮星排除半径 |
| `min_control_points` | —— | —— | 控制点数量下限 |

## 6. 接口/ABI

- entrypoint：产品组+coverage+validity → 控制点集；
- 输出被 upm 消费。

## 7. 错误与边界

- 控制点不足 → fail-closed（UPM 欠定）；
- 高结构区域误入 → 标记，不进拟合；
- 移动源区域标记（供 rejection 参考）。

## 8. 测试与 Oracle

- 构造已知背景/梯度场景 → 控制点覆盖与统计符合预期；
- 亮星/坏点排除验证；
- 欠定检测（不足时报错）。
