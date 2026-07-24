# AstroCS Baseline Manifest — P00-008 (G0 Gate)

- **Task ID**: P00-008
- **生成日期**: 2026-07-24
- **Tag 名**: `astrocs-baseline-p00`
- **Gate**: G0（基线冻结与仓库完整性）
- **Gate Status**: PASSED
- **配套文件**: `baseline_manifest.json`

---

## 1. G0 Checklist 核对

| # | 检查项 | 状态 | 证据 | 说明 |
|---|---|---|---|---|
| 1 | 13 个实际运行模块/子模块源码均受控 | PASS | P00-002 + P00-003 + P00-004 | Drizzle/Stack 源码已纳入 monorepo；13 模块在 dependency_graph.json 中全部列出 |
| 2 | 每个依赖固定版本 | PASS_WITH_CAVEAT | P00-004 dependency_graph.json + P00-005 environment_baseline.json | P00 阶段固定到当前 commit；依赖锁定清单留待 P01-002 |
| 3 | 当前工程可否构建有明确证据 | PASS_WITH_CAVEAT | P00-005 environment_baseline.json + P00-004 依赖图 | 工具链基线已采集；干净 clone 重建验证留待 P01-007；3 个路径问题已识别 |
| 4 | 旧审计已复核 | PASS | P00-006 audit_reconciliation.json | 163 项全部标记（112 OPEN / 50 CLOSED / 1 REJECTED） |
| 5 | 文档冲突已登记 | PASS | P00-007 documentation_conflict_register.json | 10 项冲突（3 high / 4 medium / 3 low），全部含来源行号与修正方向 |
| 6 | 风险和阻塞清晰 | PASS | RISK_REGISTER.csv | 10 项风险全部 OPEN，均有 mitigation_task 映射；G0 仅要求识别 |
| 7 | baseline tag 与 SHA-256 完成 | PASS | 本任务产出 | git tag `astrocs-baseline-p00` + baseline_manifest.json |

**汇总**: 5 PASS + 2 PASS_WITH_CAVEAT + 0 FAIL = **G0 PASSED**

### 1.1 PASS_WITH_CAVEAT 说明

- **检查项 2（依赖固定版本）**: P00 阶段所有源码固定到 baseline tag 指向的 commit。完整的依赖锁定清单（`dependencies.lock.json`）留待 P01-002，因 P00 不引入新构建系统，仅冻结当前状态。
- **检查项 3（构建证据）**: P00-005 已采集 16 个工具链的版本/路径/许可证/SHA-256，P00-004 已建立 13 模块 68 边依赖图。但"干净 clone 重建验证"需要 P01 的统一构建系统，P00 阶段仅记录本机当前可构建状态。3 个路径问题（GCC 不在 PATH、qmake6 不在 PATH、两个 make 并存）已识别并记录在 environment_baseline.json 中。

---

## 2. P00 任务完成情况

| Task ID | 标题 | 状态 | 关键交付物 |
|---|---|---|---|
| P00-001 | 冻结并复核主仓库基线 | DONE | preflight.json/md（基线预检报告） |
| P00-002 | 恢复并固定 healpix_drizzle 源码 | DONE | SOURCE_RECORD.md（受控源码与来源记录） |
| P00-003 | 恢复并固定 healpix_stack 源码 | DONE | SOURCE_RECORD.md（受控源码与来源记录） |
| P00-004 | 建立完整模块与依赖图 | DONE | dependency_graph.json/md（13 模块 68 边） |
| P00-005 | 采集工具链与本机环境 | DONE | environment_baseline.json/md（16 工具链） |
| P00-006 | 复核旧审计 163 项当前状态 | DONE | audit_reconciliation.json/md（112 OPEN/50 CLOSED/1 REJECTED） |
| P00-007 | 建立文档冲突登记 | DONE | documentation_conflict_register.json/md（10 项冲突） |
| P00-008 | 冻结 baseline tag | DONE | baseline_manifest.json/md + git tag `astrocs-baseline-p00` |

---

## 3. 证据文件清单

### 3.1 任务证据（42 个文件，0 missing）

详见 `baseline_manifest.json` 的 `evidence_files` 字段。每个文件记录 SHA-256 与字节数。

#### P00-001（7 文件）
- preflight.json / preflight.md / artifacts.sha256
- TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md

#### P00-002（5 文件）
- SOURCE_RECORD.md
- TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md

#### P00-003（5 文件）
- SOURCE_RECORD.md
- TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md

#### P00-004（6 文件）
- dependency_graph.json / dependency_graph.md
- TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md

#### P00-005（6 文件）
- environment_baseline.json / environment_baseline.md
- TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md

#### P00-006（6 文件）
- audit_reconciliation.json / audit_reconciliation.md
- TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md

#### P00-007（6 文件）
- documentation_conflict_register.json / documentation_conflict_register.md
- TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md

#### bootstrap（1 文件）
- INSTALL_RECEIPT.json

### 3.2 控制文件（10 个文件）

详见 `baseline_manifest.json` 的 `control_files` 字段。

- engineering/control/MASTER_TASK_REGISTER.csv
- engineering/control/PROJECT_STATE.yaml
- engineering/control/CURRENT_WORK.md
- engineering/control/DECISION_LOG.md
- engineering/control/RISK_REGISTER.csv
- engineering/control/AUTONOMY_POLICY.md
- engineering/control/CHANGE_CONTROL.md
- engineering/control/DATASET_REGISTER.csv
- engineering/control/INTERFACE_REGISTER.csv
- engineering/control/REQUIREMENTS_TRACEABILITY.csv

---

## 4. 风险与 ADR 状态

### 4.1 风险登记表（RISK_REGISTER.csv）

| 风险 ID | 描述 | 影响 | 可能性 | 缓解任务 | 状态 |
|---|---|---|---|---|---|
| R-001 | 核心 Drizzle/Stack 源码未纳入导出基线 | Critical | High | P00-002;P00-003 | OPEN（P00 已纳管，待 P01 构建验证后 CLOSED） |
| R-002 | Stage 2 空成功节点导致伪完成 | Critical | High | P06-001 | OPEN |
| R-003 | HISS/HCSD 格式漂移和兼容性不足 | High | High | P02-003;P02-004 | OPEN |
| R-004 | 跨 DLL 内存所有权不清 | High | Medium | P03-002 | OPEN |
| R-005 | 配置字段静默不生效 | High | High | P03-005 | OPEN |
| R-006 | 真实测试数据无版本与校验和 | High | High | P02-006 | OPEN |
| R-007 | PipelineFrame 双实现漂移 | High | Medium | P02-001 | OPEN |
| R-008 | float32 到 uint16 截断影响检测和 PSF | High | High | P05-004 | OPEN |
| R-009 | 旧审计结论未按当前源码复核 | Medium | High | P00-006 | OPEN（P00 已复核，待 P01+ 修复后 CLOSED） |
| R-010 | 无 CI/统一构建导致本机可用但不可复现 | High | High | P01-007;P04-005 | OPEN |

**G0 阶段仅要求风险已识别**，10 项风险均有 mitigation_task 映射，满足 G0 通过条件。

### 4.2 ADR 状态（DECISION_LOG.md）

| ADR | 状态 | 主题 | 关联冲突 |
|---|---|---|---|
| ADR-001 | PENDING | Drizzle/Stack 源码纳管 | C-001 模块仓库列 |
| ADR-002 | PENDING | PipelineFrame 唯一所有者 | C-006 data_pipeline 归属 |
| ADR-003 | PENDING | Stage 2 节点模型 | C-004 Stack 节点 |
| ADR-004 | PENDING | 根级构建策略 | — |

4 项 ADR 均 PENDING，将在 P01-P06 阶段陆续决策。G0 阶段不要求 ADR 完成。

---

## 5. Baseline Tag

- **Tag 名**: `astrocs-baseline-p00`
- **Tag 类型**: annotated tag
- **指向 commit**: P00-008 提交后的 HEAD
- **创建命令**: `git tag -a astrocs-baseline-p00 -m "..." <commit>`
- **Tag 包含**: G0 证据摘要 + P00-001 ~ P00-007 交付物索引

---

## 6. G0 通过声明

基于以上证据：

1. P00-001 ~ P00-007 全部 DONE，42 个证据文件齐全（0 missing）
2. G0 Checklist 7 项全部 PASS 或 PASS_WITH_CAVEAT（0 FAIL）
3. 2 项 PASS_WITH_CAVEAT 的后续工作（P01-002 依赖锁定、P01-007 干净 clone 重建）已明确归属，不影响 G0 通过
4. 10 项风险已识别且有 mitigation_task 映射
5. 4 项 ADR 已登记，将在后续阶段决策
6. baseline tag `astrocs-baseline-p00` 已创建

**G0 Gate: PASSED**

下一阶段 P01（可复现构建）可以开始。
