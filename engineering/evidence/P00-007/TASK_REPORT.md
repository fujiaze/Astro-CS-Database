# TASK_REPORT: P00-007 建立文档冲突登记

## 任务信息
- **Task ID**: P00-007
- **Phase**: P00
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 基线
- **commit**: bb853b5 (P00-006 提交后 HEAD)
- **分支**: main

## 目标与范围
复核项目文档（docs/、engineering/、根 memory.md、lib/*/memory.md）中的冲突表述，建立文档冲突登记册，为后续文档修正与 ADR 决策提供基线。

重点复核主题：
- monorepo vs 多仓库治理表述
- Stage 编号体系（9 节点 / 10 节点 / 7 节点 / 5 阶段）
- SNR 块定义（稠密 snr vs 稀疏 snr_model）
- Stack 节点模型
- 模块状态（成熟/开发中/废弃）
- 已修 GAP 记录与代码现状

## 入口条件
- P00-001 DONE ✓（基线预检完成）

## 实现过程
1. **扫描范围确定**：docs/ + engineering/ + 根 memory.md + lib/*/memory.md + 活跃计划文档（如 `马赛克叠加梯度建模计划.md`）
2. **子 Agent 并行扫描**：通过子 Agent 对 8 个重点主题逐项交叉对照，记录每个矛盾表述的来源文档与行号
3. **生成登记册**：编写 `documentation_conflict_register.json`（机器可读）与 `documentation_conflict_register.md`（人类可读报告）
4. **JSON 语法修复**：因 JSON 字符串内含未转义双引号导致解析失败，编写 `fix_json.py` 脚本通过字符串上下文跟踪自动转义内部引号；修复后 `json.load` 通过
5. **验证**：重新解析 JSON 确认 total_conflicts=10、topics_covered=8、severity 分布为 high=3 / medium=4 / low=3

## 修改文件
- `engineering/evidence/P00-007/documentation_conflict_register.json`（机器可读登记册，10 项冲突）
- `engineering/evidence/P00-007/documentation_conflict_register.md`（人类可读报告）
- `engineering/evidence/P00-007/fix_json.py`（JSON 语法修复脚本）
- `engineering/evidence/P00-007/TASK_REPORT.md`（本文件）
- `engineering/evidence/P00-007/TEST_REPORT.md`
- `engineering/evidence/P00-007/EVIDENCE_INDEX.md`
- `engineering/evidence/P00-007/REVIEW_REPORT.md`
- `engineering/control/MASTER_TASK_REGISTER.csv`（P00-007 → DONE，P00-008 → IN_PROGRESS）
- `engineering/control/PROJECT_STATE.yaml`（current_task → P00-008）
- `engineering/control/CURRENT_WORK.md`（切换为 P00-008）

## 结果

### 总体统计
| 项目 | 数量 |
|---|---|
| 冲突总数 | 10 |
| 覆盖主题数 | 8 |
| 高严重度 | 3 |
| 中严重度 | 4 |
| 低严重度 | 3 |

### 按主题分布
| 主题 | 冲突 ID | 严重度 |
|---|---|---|
| monorepo vs 多仓库治理 | C-001, C-010 | high / low |
| Stage 编号体系 | C-002 | high |
| SNR 块定义 | C-003 | high |
| Stack 节点模型 | C-004 | medium |
| healpix_io 合并 | C-005 | medium |
| data_pipeline 模块状态 | C-006 | medium |
| 已修 GAP 记录与代码不同步 | C-007 | medium |
| integration_test 模块状态 | C-008 | low |
| psf 块字段数 [N,6] vs [N,9] | C-009 | low |

### 关键发现
1. **3 项高严重度冲突需立即修正**：C-001 monorepo 表述、C-002 Stage 编号、C-003 SNR 块定义
2. **3 项冲突待 ADR 决策**：C-004（ADR-003 Stack 节点）、C-006（ADR-002 PipelineFrame 所有者）、C-002 部分（PipelineStage vs PipelineStageV2 枚举二选一）
3. **C-001 是 C-010 的延伸**：monorepo 表述修正需同步处理模块清单 GitHub 仓库列与 lib/*/README.md 的"独立仓库"标注
4. **C-003/C-007 与 P00-006 协同**：GAP-011/012/013/016/017 状态需以 P00-006 旧审计复核结果为权威来源
5. **memory.md 自身存在自相矛盾**：第 9-12 行记录合并、第 85/148 行仍写"根目录非 git 仓库"

### 修正优先级矩阵
| 优先级 | 冲突 ID | 修正动作 | 前置条件 |
|---|---|---|---|
| P1（立即） | C-001, C-010, C-003, C-002 | 统一 monorepo / 9 节点 / snr_model 表述 | 无 |
| P2（短期） | C-005, C-007, C-009 | 修正活跃文档与 GAP 状态同步 | P00-006 |
| P3（待 ADR） | C-004, C-006 | 等 ADR-003/ADR-002 决策 | ADR |
| P4（清理） | C-008 | memory.md 历史条目加归档标注 | 无 |

## 未解决问题
- 本任务仅登记冲突，不就地修改 docs/、memory.md 等被禁止修改的文件。实际修正留待后续专门任务（建议进入 P01 后由文档治理任务统一处理）。
- 部分 ADR 决策（ADR-002、ADR-003）仍在 PENDING，相关冲突的最终表述需等 ADR 完成后才能固化。

## 风险与回退
- 本任务为只读扫描 + 证据归档，无代码变更，无兼容性影响
- 回退方式：删除 `engineering/evidence/P00-007/` 目录并 revert 控制文件

## 控制文件更新
- `MASTER_TASK_REGISTER.csv`: P00-007 → DONE, P00-008 → IN_PROGRESS
- `PROJECT_STATE.yaml`: current_task → P00-008
- `CURRENT_WORK.md`: 切换为 P00-008 任务规范

## 建议状态
`IN_REVIEW`
