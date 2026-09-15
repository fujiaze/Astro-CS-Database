# Phase2 PSFSW 算法规格包（ALG-P2-PSFSW-001）

文档包 ID：`ALG-P2-PSFSW-001`
任务：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/ALG-P2-PSFSW-001.md`（wave 3，`depends_on = SCI-ADJ-001`）
write_scope（tracked）：`docs/algorithms/v6/phase2-psfsw/`（本目录）
write_scope（workspace，gitignore）：`run/v6/alg-p2-psfsw/`（机器可读规格、Oracle、门、日志、证据）
基线：`HEAD = main = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`（任务派发时 `git rev-parse HEAD` 实测）
性质：**算法规格**（可实施、可验证的冻结建议输入），不是实现、不是 schema、不写生产代码；正式冻结由 wave 4 `CONTRACT-FREEZE-001` 写入 `docs/science/v6/frozen/`、`docs/contracts/v6/frozen/`、`docs/algorithms/v6/frozen/`。

## 0. 本包文件索引

| 文件 | 内容 | 说明 |
|---|---|---|
| `README.md` | 本文件 | 入口、权威、纪律、验证 |
| `PSFSW_ALGORITHM_SPEC.md` | 主规格 | 共同星集/四分量/复合与稳健归一/版本化/conventional coadd/covariance/effective PSF/基线比较/fail-closed/provenance/验证门（每条规格带条款锚） |
| `PSFSW_FROZEN_THRESHOLDS.md` | 冻结阈值册 | 深度稳定性阈值、`n_common` 下限与分档、空间非均匀/趋势门、功率损失门；含数值来源、验证门与 W4 确认要求 |
| `PSFSW_BASELINE_COMPARISON.md` | 基线比较协议 | 等权/exposure/pixel-ivar/`W_info` 四基线的权重定义、预注册、统计声明门 |
| `PSFSW_GATES_AND_MUTATIONS.md` | 门目录与负向 mutation 册 | PSFSW-G01..G25 机器判据、fail-closed 白名单、REJECT 集、负向注入清单 |

机器可读规范源（唯一数值源）：`run/v6/alg-p2-psfsw/spec/psfsw_spec.json`（`astrocs.phase2.psfsw-algorithm-spec/v1`）。人读正文与它的一致性由独立 Oracle 的 V1 结构检查核对（阈值 ID、门目录、fail-closed 白名单、模式枚举、tracked 文档存在性）；以 JSON 为可执行规范源、Markdown 为可读渲染。

## 1. 权威分层（冻结宪章 §1.1）

1. `ASTROCS_PROJECT_CONSTITUTION.md`（FROZEN）§3.2/§4.1/§4.3/§6.3/§13.1/§14.5/§18；
2. `docs/owner/PROJECT_SPEC.md` §3/§4/§5/§7/§8；
3. `docs/design/PHASE2_DETAILED_DESIGN.md` §6.3/§7/§9/§10 与 `docs/design/PHASE1_DETAILED_DESIGN.md` §8.1/§8.2/§8.3/§9/§11；
4. `docs/science/UNIFIED_SCIENCE_MODEL.md` §2..§11 与 `docs/science/PSF_SIGNAL_WEIGHT.md` §1..§8；
5. 科学冻结 `SCI-ADJ-001`（`FREEZE_LIST` / `CONFLICT_MATRIX` / `adjudications.json`）与 Wave 1 复核（`docs/science/v6/{psfw,phase2,observation,phase3}/`、`reports/v6/review-audit/`）；
6. 控制器裁决 `CONTROLLER_LOG.md` C-004（本任务全部遵守，未推翻任何一条）。

本任务与 `CONTRACT-FREEZE-001`(W4) 的关系：本包给出**可实施、可验证**的算法冻结建议并落定 W1 复核显式委托给 W3 的数值（深度稳定性阈值、`n_common` 分档、空间非均匀判据、复合指数/常数版本）；正式冻结由 W4 写入 frozen 目录；schema 词表归一由 `SCHEMA-INTEGRATE-001`(W6) 执行（`C-004.3`），本包不写 schema。

## 2. 强制纪律声明

- 只写上述两处 write_scope；未修改 `docs/science/*.md`、`docs/owner/**`、`docs/design/**`、`docs/references/**`、生产源码、根 CMake、其他任务文件（证据：`run/v6/alg-p2-psfsw/logs/30_scope_check.log`，与任务开始前基线 dirty 快照比对）。
- 未 commit / push / add / 分支 / worktree / stash / reset / clean / rebase；git 仅只读使用（`evidence/baseline_dirty_porcelain.txt` 为只读快照）。
- 未派生子代理（未调用 subagent/subagent_fork/ralph）。
- 未宣布发布；未修改任何既有冻结门/容差；`SO-01..07` 需负责人签字项只登记不擅改；`CTRL-F1`、`CTRL-AR033` 控制器级事项只登记不裁决。
- 所有外部命令检查退出码并留日志（`run/v6/alg-p2-psfsw/logs/`）。

## 3. 验证与复现（一键）

```bash
cd /workspace/Astro\ CS\ Database
bash run/v6/alg-p2-psfsw/tools/run_all.sh   # 全部门 + 负向 mutation + Oracle + 审计，单一 rc
```

实测退出码见 `run/v6/alg-p2-psfsw/summary.json` 与 `logs/*.log`；负向 mutation 注入错误必须 rc!=0（PSFSW-G 门目录见 `PSFSW_GATES_AND_MUTATIONS.md`）。

## 4. 建议状态

本任务建议状态：**PASS**（交付物经独立 Oracle、正/负向门、结构审计、越界审计自验；详见各文件与 `run/v6/alg-p2-psfsw/summary.json`）。最终验收由控制器按任务卡执行。