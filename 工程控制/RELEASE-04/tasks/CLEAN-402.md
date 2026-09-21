# 任务：CLEAN-402 历史治理工件清理

## 目标
仓库只保留最新生产代码、自解释文档集与当前控制包；历史控制包、一次性报告与归档目录在结论沉淀进正式文档后清理。

## 权威依据
- ENGINEERING_SPEC.md §9（仓库整洁与治理工件清理）
- CONTROL_PACK_SPEC.md §9（收口即清理）
- GAP_AUDIT G2-4

## 改动范围（文件域，互斥）
- 允许改/删：`工程控制/RELEASE-01`、`RELEASE-02`、`RELEASE-03`、`PROJECT-GOVERNANCE-01`、`PROJECT-GOVERNANCE-02`、`SCI-RES-01`、`reports/`（历史批次）、`docs/archive/`、`docs/backlog/`、`docs/governance/`、`docs/owner/`、`docs/review/`（先审计）
- 禁止改：代码、正式科学文档、testdata、外部数据集目录、当前控制包 RELEASE-04

## 步骤
1. 审计上述每个目录：列文件清单、内容摘要、其中仍有长期价值的结论（已定案科学口径、门禁规则、验收结论、已知限制）；
2. 有长期价值的结论逐条并入正式文档（科学结论→science/algorithms；门禁→docs/ci；已知限制→docs/KNOWN_LIMITATIONS；验收状态→RELEASE_STATUS 或验收报告），并入位置登记成对照表；
3. 删除历史控制包目录与一次性报告（过程由 git 历史承载）；docs 下 archive/backlog/governance/owner/review 中，已被正式文档吸收的删除，确有现行用途的重新归位到正式目录并更新索引（DOC-403 联动）；
4. 清理后全仓搜索对被删文件的引用（文档、CMake、脚本、检查器白名单），逐条消除；
5. 拿不准是否可删的（可能是在用的台账/清单）列出"保留候选清单 + 理由"上呈，不私自删；
6. 清理单独成提交（可按目录分多个提交），提交信息附删除清单与结论去向对照表。

## 验收门（可机器复跑）
- [ ] `工程控制/` 下仅保留 RELEASE-04（执行中）；
- [ ] 结论去向对照表存在且每条被删结论可在正式文档中定位；
- [ ] 悬空引用门（DOC-403 的 doc-index）全绿；
- [ ] `ci --all`、ctest 全绿（清理不影响构建）；
- [ ] 保留候选清单已上呈并获答复。

## 禁止
- 不删任何代码（代码清理归 CLEAN-401）；
- 不删 testdata/、根目录外部数据集、实验/ 目录；
- 不把历史报告原文搬进正式文档（只沉淀现行结论）。
