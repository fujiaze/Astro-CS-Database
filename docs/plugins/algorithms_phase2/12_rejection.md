# 插件文档：rejection（排异推断）

## 1. 职责与边界

- **职责**：估计潜在污染状态（cosmic ray、卫星线、坏列、移动源、云/梯度、失焦/拖线），输出 mask/count/reason/probability。
- **不是**：不是把异常值变成零；不是 coverage；移动源等科学信号可选择保留到独立层，**不默认当缺陷删除**。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.4（排异是污染状态的估计）
- `docs/science/REJECTION.md`、`docs/design/PHASE2_DETAILED_DESIGN.md` §5

## 3. 输入/输出数据合同

- **输入**：归一化产品组、预测残差方差（含 Phase1 噪声 + UPM 参数不确定度）、validity、配置。
- **输出**：rejection mask、count、reason 分类、probability、方法版本。
- 参考：`contracts/schemas/rejection_output.schema.json`。

## 4. 算法与公式要点

- 分类型估计：cosmic ray、卫星线、坏列、移动源、云/梯度、失焦/拖线；
- 阈值使用**预测残差方差**（包含 Phase1 噪声与 UPM 参数不确定度），不得用固定全局阈值；
- 小样本规则、迭代上限、方法版本化；
- 输出 mask/count/reason/probability；
- 移动源等科学信号 → 独立层保留选项。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `rejection_classes` | 全类 | —— | 启用的排异类 |
| `sigma_gate` | —— | σ | 预测残差阈值 |
| `max_iter` | —— | —— | 迭代上限 |
| `keep_moving_sources` | true | —— | 移动源独立层保留 |

## 6. 接口/ABI

- entrypoint：归一化产品组 → rejection 产品；
- 输出被 integration 消费（作为门/概率）。

## 7. 错误与边界

- 预测残差方差缺失 → fail-closed（不能用猜测阈值）；
- 小样本 → 明确规则（不静默删）；
- 方法版本必须记录（改变排异策略改变版本）。

## 8. 测试与 Oracle

- 注入各类污染（cosmic ray/卫星线/坏列/移动源/云）→ 检测与分类符合；
- 预测残差阈值正确性（含 UPM 不确定度）；
- 移动源保留到独立层验证；
- 小样本规则与迭代上限。
