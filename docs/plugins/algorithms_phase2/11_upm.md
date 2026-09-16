# 插件文档：upm（统一相对模型）

## 1. 职责与边界

- **职责**：对重叠区域拟合每帧光度尺度修正与加性背景的统一相对模型（UPM），并施加归一化。
- **不是**：不做排异；不做集成；`g_k`（乘法）与 `b_k`（加性）**不得互相代替**；校准参数不确定度必须传播。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.4（UPM）
- `docs/design/PHASE2_DETAILED_DESIGN.md` §4
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（参数协方差）

## 3. 输入/输出数据合同

- **输入**：Phase1 产品组、coverage、控制点集。
- **输出**：每帧 `g_k`、`b_k` 及协方差、gauge 约束、秩/连通性/条件数、残差、拟合质量；施加归一化后的产品。
- 参考：`contracts/schemas/upm_output.schema.json`。

## 4. 算法与公式要点

```text
y_k(x) = g_k s(x) + b_k(x) + ε_k(x)
```

- `g_k` 乘法响应、`b_k` 加性背景；不得互相代替；
- 约束 gauge（固定参考帧或和约束），报告秩、连通性、条件数、残差、参数协方差；
- 控制点避开源/饱和/坏点/高结构（sampling 保证）；
- 校准参数不确定度传播到最终 covariance；
- 欠定、断图或显著模型失配 → 显式失败/分组件。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `gauge` | `reference_frame` | —— | reference_frame / sum |
| `bkg_model_order` | 0 | —— | 加性背景空间阶数（0=常数） |
| `max_iter` | —— | —— | 稳健拟合迭代上限 |
| `convergence_gate` | —— | —— | 收敛门 |

## 6. 接口/ABI

- entrypoint：产品组+控制点 → UPM 参数+协方差+归一化产品；
- 归一化施加输出被 rejection/integration 消费。

## 7. 错误与边界

- 断图/欠定 → 显式失败或分组件，不假装同基准；
- 显著模型失配 → 失败；
- 参数协方差不输出 → 下游 covariance 不可信，标记。

## 8. 测试与 Oracle

- 构造已知 `g_k/b_k` 场景 → 参数恢复符合精度；
- 断图/欠定能红；
- 参数不确定度传播到最终 covariance 验证；
- 与独立高精度矩阵 oracle 对比。
