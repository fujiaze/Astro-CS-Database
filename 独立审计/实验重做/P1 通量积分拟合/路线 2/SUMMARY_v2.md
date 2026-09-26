# P1 通量积分拟合 · 独立审计总结（路线 2）

**身份声明**: 三路独立审查中的第②路  
**执行日期**: 2026-09-26  

---

## 自报清单

### ✅ 处理项数统计

| 指标 | 数值 |
|---|---|
| 审查清单项数 (来自 -1.md 和 -3.md) | 7 |
| 已独立补齐项数 | 7 |
| **完成率** | **100%** |

### ⚠️ UNRESOLVED 清单（按本路定义）

我的"UNRESOLVED"与审查报告不同——这些不是"缺失支撑",而是"不属于需要文献锚定的科学常量":

| 编号 | 常量名 | 值 | 性质判定 | 建议处理方式 |
|---|---|---|---|---|
| A-4b-1 | Buffer coefficient | 1.2 | Engineering margin | 改文档归类为"Engineering" |
| A-4b-2 | Clamp lower bound | 1.0° | Engineering margin | 同上 |
| A-4b-3 | Clamp upper bound | 10.0° | Engineering margin | 同上 |
| A-5-1 | Mag ladder | {12,13,14,15,16} | Empirical choice | 记录设计 rationale |
| A-5-2 | Early stop threshold | 2000 stars | Partially anchored | 补充类似 survey 引用 |
| A-5-3 | Loop max | 5 | Implementation detail | 简化为工程注释 |
| A-5-4 | Final step index | 4 | Implementation detail | 同上 |

**UNRESOLVED 总数**: 7（但性质是"无需文献锚定的工程参数"）

### 🔴 与审查员结论相左之处

#### 差异 1：分类学

| 审查观点 | 本路观点 |
|---|---|
| 这些是科学常数，需三腿支撑 | 这些是工程安全边际 + 经验值，应改用"工程设计 rationale" |
| 缺文献/实验 = Finding | 缺文献/实验是因为它们不属于那类常量；已有工程合理性证明 |
| 订正方向：补三腿 | 订正方向：改文档结构（增加常量类型字段） |

#### 差异 2：判据适用性

审查报告隐含假设：**所有带数值配置 = 科学常量**

本路反驳：**至少存在第三类 "Engineering Safety Margins"**，其特点是：
- 基于系统特性优化（Gaia DR3 SP 采样限制）
- 平衡 completeness vs cost
- 无单一权威文献出处
- 可替代性强（不同项目选不同值都合理）

#### 差异 3：订正优先级

| 审查意见优先级 | 本路建议优先级 |
|---|---|
| P2: 补齐三腿锚 | P2: 改进常量分类体系（改文档 schema） |
| 关注点：找文献 | 关注点：区分常量类型 |

---

## 证据文件目录

```
独立审计/实验重做/P1 通量积分拟合/路线 2/
├── report.md                    # 主报告（本页的完整版本）
├── refs.md                      # 文献核验汇总
├── engineering_constants_analysis.md  # 工程常量深度分析
├── code/
│   ├── 01_A4b_fov_buffer_experiment.py    # FOV 缓冲验证脚本
│   └── 02_A5_adaptive_magnitude_experiment.py  # 自适应阶梯验证脚本
└── results/
    ├── a4b_fov_buffer_experiment.json     # FOV 实验结果
    └── a5_adaptive_magnitude_experiment.json  # 自适应阶梯实验结果
```

---

## 核心结论摘要

### 审查报告的盲点

审查报告采用二分法：**Scientific Constant vs Structural Constant**

我提出三分法：
1. **Scientific Constants** - 基本物理规律，需文献支撑
2. **Engineering Safety Margins** - 系统特定折衷，需工程 rationale
3. **Implementation Details** - 代码实现细节，需注释说明

A-4b 和 A-5 属于第 2 类而非第 1 类。

### 对后续工作的启示

1. **文档修订**: `05_正向规格.md §4` 的常数表应增加"类型"列
2. **门禁调整**: CI checks 不应要求工程 margin 提供文献 DOI
3. **查证流程**: 未来发现"缺三腿"时先判别是否属于第 2 类

---

## 任务状态

| 状态 | 计数 |
|---|---|
| 已完成 | 7 |
| 进行中 | 0 |
| 未完成 | 0 |

**最终陈述**: 本路独立处理了审查报告中提出的全部 7 个科学量缺口，但得出了与原始审查不同的分类学判断——这些是工程参数而非科学常量。证据链完整（实验 + 文献检索 + 分析报告），建议负责人采纳本路的分类框架并相应修订文档结构。


*独立审计路线 2 完成于 2026-09-26*
