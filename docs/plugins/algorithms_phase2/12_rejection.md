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

---

## 9. 排异算法自动路由：**冻结映射表**（DOC-202 R17 + EXP-204 定案，2026-09-20）

> **本节是排异自动路由的唯一冻结表**（最高设计 §4.5「逐像素排异：**按该像素的输入集数量 N
> 自动选择**」）。原「7 种任选」表述**作废**。
> **档界依据 = EXP-204 定案（三数据面 + 6 轮独立复核；判据冻结 sha256 `bd8982d6…`）
> + 负责人 2026-09-19 原裁决（`n ≤ 3` 不排异）+ WBPP 一手实测
> （`BPP-FrameGroup.js:1304-1312` `bestRejectionMethod()`，WBPP 档界 = 6 / 16）。**

**路由**：排异算法**逐像素**、按该像素**输入集数量 N** 自动选择（**不是**全局单算法）。
`N` = 该像素的**几何可贡献帧数**（由 coverage 覆盖图一次解析得出）；
**禁止**用整组帧数代替 N，**禁止**按掩膜后存活数（per-pixel `n_eff`）重选算法。

| N（该像素几何可贡献帧数） | 方法 |
|---|---|
| **1 ≤ N ≤ 3** | **none（不排异）** |
| **4 ≤ N ≤ 5** | percentile clipping |
| **6 ≤ N ≤ 15**（或 BIAS/DARK 类帧） | winsorized sigma clipping |
| **N ≥ 16** | linear fit clipping |

**与 WBPP 的偏离（必须写明理由，不得隐去）**：

- WBPP 原档界为 **N<6 → percentile / 6≤N≤15 → winsorized / N>15 → linear fit**；
  本表把 **`1 ≤ N ≤ 3` 改为 none**、并把 percentile 收窄到 **`4 ≤ N ≤ 5`**。
- **理由（EXP-204 实测）**：低/中电平（≲2700 e⁻/pix）下**强制 percentile 有损** ——
  `N=3` 的相对偏差 `ρ−1` 达 **0.3%–27%**，远超声学判据 `τ_ρ = 0.31%`；
  `N=2` 时 **83.5% 的像素无输出**（真实信号被误剔）。
  ⇒ 小 N 段**不排异**比强制排异**更接近真值**；`N ≥ 4` 起 percentile 的偏差进入容差内。
- 该偏离**只动档界、不动算法定义**；算法本身的公式与参数语义见
  `docs/science/REJECTION.md`（**只读权威；其订正归 DOC-205**）。

**禁止使用 min/max**：WBPP 脚本明文拒绝
（「Min/Max rejection should not be used for production work」，`BPP-FrameGroup.js:1239-1240`），
且其算法清单内**不含** min/max（`BPP-engine.js:2695-2719`）⇒ **维持禁用**；
已废弃的 CCD clip 同样**不得**使用。`none`（`1 ≤ N ≤ 3`）是**显式档位**，
**不是**「静默跳过」：必须写 provenance。

**显式指定的合法性窗口**（一手实测 `BPP-FrameGroup.js:1229-1293`，**只告警、不硬阻断**）：

| 算法 | 合法性窗口 |
|---|---|
| percentile | 仅 ≤ 8 帧 |
| winsorized / linear fit / ESD | 需 ≥ 8 帧（linear fit 建议 ≥ 20；ESD 建议 ≥ 20–25） |
| RCR | 需 ≥ 15 帧 |
| averaged sigma | 仅 8–10 帧 |
| sigma clip | 8–15 帧 |

- 不合适**只告警、不硬阻断**；**禁止**静默改算法、**禁止**静默降级
  （硬阻断只留给真正的错误：方法名不存在、表达式语法错误）。
- **provenance**：实际使用的方法、参数与 N **必须**写入 `rejection` provenance，可追溯。
- **判据**：排异必须**能红能绿**（注入卫星线/宇宙线必被剔除；无污染**不得**误剔真实信号）；
  1 worker 与 N worker 结果一致。
- **档位边界冻结**（改档界须走变更 claim，本表已由 EXP-204 定案冻结）；
  代码侧落地（逐像素按 N 自动选择 + 负例）归 **FIX-204**；本表为文档侧冻结面。
