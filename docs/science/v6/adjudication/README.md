> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# SCI-ADJ-001 — 科学裁决整合（Wave 2，人读正文索引）

> 上游：ASTROCS_DESIGN.md §2（核心科学方法）、§3（数据对象与配置）

- 任务：`工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/SCI-ADJ-001.md`
- 文档 ID：`SCI-ADJ-001-ADJUDICATION`
- 基线：`HEAD = main = bc166e9d4828b45ef32b679156e12d954640534a`（本机 `git rev-parse` 实测）
- 性质：**整合裁决（integration adjudication）**——消费五份 Wave 1 交付 + 控制器 C-001..C-005 裁决，列冲突矩阵，冻结字段/公式/mode/适用域/降级/验证门。**只读输入**；不改任何上位规范、不改公式/容差/冻结门、不写生产代码、不 commit/push。
- 建议状态：**REVIEW_REQUIRED**（本体逐项完成、门可红，但 7 项 `owner_signoff_required` 与 5 项 `controller_only` 超出本任务权限，须负责人/控制器签字，见冲突矩阵 §5/§6）。

## 文件

| 文件 | 内容 |
|---|---|
| `README.md` | 本索引 |
| `SCI-ADJ-001_CONFLICT_MATRIX.md` | 冲突矩阵：冲突项 → 各方立场 → 控制器/本任务裁定 → 生效条款；含只登记不裁决项与需负责人签字项 |
| `SCI-ADJ-001_FREEZE_LIST.md` | 冻结清单：字段 / 公式 / mode / 适用域 / 降级 / 验证门，逐条带条款锚（由 JSON 机械渲染） |

## 机器可读伴生

| 文件 | 内容 |
|---|---|
| `reports/v6/science-adjudication/adjudications.json` | 24 条裁决 + 42 条冻结条目 + 单位表 + mode 表 + 7 项签字项 + 5 项控制器事项；schema `astrocs.v6.sci-adjudication/v1` |
| `reports/v6/science-adjudication/SUMMARY.md` | 上述机器摘要的人读索引（由 JSON 机械渲染） |
| `run/v6/adjudication/tools/oracle_adj.py` | 独立 Oracle（纯 numpy，不调用生产实现）：面亮度归一/pixfrac 不变量、通量守恒、方差缩放、GLS vs 像素 ivar、PSFSW 方差来源 |
| `run/v6/adjudication/tools/check_adjudication.py` | 冻结表结构一致性检查器（枚举合法 / 单位表一致 / 禁止项缺省即红 / required freeze 缺失即红 / psfsw 边界 / C-004 消费） |
| `run/v6/adjudication/tools/check_doc_consistency.py` | 人读正文 ↔ 机器表交叉引用检查（含注入假 id 的 selftest） |
| `run/v6/adjudication/tools/mutate_and_check.py` | 冻结表负向 mutation 驱动（删冻结 / 改单位 / psfsw 写 ivar / median SNR 入权重 → 期望 rc!=0） |
| `run/v6/adjudication/tools/run_all.py` | 一键复跑（单一 rc） |
| `run/v6/adjudication/evidence.json` | 机器汇总（基线 SHA、check 结果、mutation 结果、rc） |
| `run/v6/adjudication/logs/` | 全部命令 stdout/stderr 与 rc |

## 输入（逐份消费）

1. `docs/science/v6/observation/OBSERVATION_MODEL_REVIEW.md`（F-OBS-01..05）
2. `docs/science/v6/psfw/PSFW_FREEZE_RESEARCH.md`（R1..R8 / P1..P18）
3. `docs/science/v6/phase2/SCI-P2-001_THREE_MODE_REVIEW.md` + `COVARIANCE_AND_EFFECTIVE_PSF.md` + `WEIGHT_PROVENANCE_GATE.md`
4. `docs/science/v6/phase3/PHASE3_PROPAGATION_REVIEW.md`（F3-01..06）
5. `reports/v6/review-audit/00..06`（AR-001..051）
6. 控制器 `CONTROLLER_LOG.md` C-001..C-005

## 纪律声明

- 未 commit / push / git add；未建分支/worktree；未 stash/reset/clean/rebase；git 仅只读。
- 未越界写：仅写 `docs/science/v6/adjudication/`、`reports/v6/science-adjudication/`、`run/v6/adjudication/`（`run/*` gitignore）。
- 未派生子代理（未调用 subagent/subagent_fork/ralph）。
- 未宣布发布；未修改科学公式、容差或冻结门；未推翻任何控制器裁决（C-004 全 6 条遵守）。
- F1（git 基线分歧）与 AR-033（构建面 owner）只登记不裁决（控制器级）。
