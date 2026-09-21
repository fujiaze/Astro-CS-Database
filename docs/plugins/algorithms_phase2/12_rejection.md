# 插件文档：rejection（排异推断）

> 上游：ASTROCS_DESIGN.md §5.2（固定科学流程）

## 1. 职责与边界

- **职责**：估计潜在污染状态（cosmic ray、卫星线、坏列、移动源、云/梯度、失焦/拖线），输出 mask/count/reason/probability。
- **不是**：不是把异常值变成零；不是 coverage；移动源等科学信号可选择保留到独立层，**不默认当缺陷删除**。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §5.5（逐像素排异：按几何覆盖帧数 N 自动选择算法）
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

---

## 9. 排异算法自动路由：按几何覆盖帧数 N 的档位表

**路由**：排异算法**逐像素**、按该像素的**几何可贡献帧数 N** 自动选择（**不是**全局单算法）。
`N` = 该像素的**几何可贡献帧数**（由 coverage 覆盖图一次解析得出）；
**禁止**用整组帧数代替 N，**禁止**按掩膜后存活数（per-pixel `n_eff`）重选算法。

| N（该像素几何可贡献帧数） | 方法 |
|---|---|
| **1 ≤ N ≤ 3** | **none（不排异，直接逆方差加权积分）** |
| **4 ≤ N ≤ 5** | percentile clipping |
| **6 ≤ N ≤ 15** | winsorized sigma clipping |
| **N ≥ 16** | linear fit clipping |

- 生产排异算法集 = none / percentile / winsorized / linear fit；**min/max 极值法不用于生产**
  （WBPP 一手源码明文拒绝：`BPP-FrameGroup.js:1239-1240`；其算法清单亦不含 min/max：`BPP-engine.js:2695-2719`）。
- `none` 是**显式档位**，**不是**「静默跳过」：必须写 provenance。
- 本表档界与 WBPP 档界的差异及其实验依据（低/中电平下强制 percentile 的有损性）见
  `docs/science/REJECTION.md`；算法本身的公式与参数语义同样以该文件为唯一正本（只读权威）。

**显式指定的合法性窗口**（`BPP-FrameGroup.js:1229-1293`，**只告警、不硬阻断**）：

| 算法 | 合法性窗口 |
|---|---|
| percentile | 仅 ≤ 8 帧 |
| winsorized / linear fit / ESD | 需 ≥ 8 帧（linear fit 建议 ≥ 20；ESD 建议 ≥ 20–25） |
| RCR | 需 ≥ 15 帧 |
| averaged sigma | 仅 8–10 帧 |
| sigma clip | 8–15 帧 |

- JSON 中排异字段留空或 `auto` 时按上表逐像素路由；显式指定单一算法时按指定执行；
  指定算法与该 N 的适用域冲突时报 warn 并请确认；方法名不存在或表达式非法报 error。
- 不合适**只告警、不硬阻断**；**禁止**静默改算法、**禁止**静默降级。
- **provenance**：实际使用的方法、参数与 N **必须**写入 `rejection` provenance，可追溯。
- **判据**：排异必须**能红能绿**（注入卫星线/宇宙线必被剔除；无污染**不得**误剔真实信号）；
  1 worker 与 N worker 结果一致。
- NaN 采用**样本级掩膜**：污染样本掩除后**重归一**、覆盖级缺数置 NaN 并**强制计数**
  （规则见 `docs/science/REJECTION.md`）。
