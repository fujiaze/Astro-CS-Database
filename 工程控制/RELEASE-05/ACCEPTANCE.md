# ACCEPTANCE — RELEASE-05 执行状态登记（执行 agent 维护）

## 任务状态

| ID | 状态 | 提交/证据指针 | 偏差登记 |
|---|---|---|---|
| DOC-501 | PASS | 3f5265ff、bf1bc15b；门：DOC-INDEX/DOC-INDEX-SELFTEST/CHK-STALE-DOC/DOC-L0/CHK-REGISTRY-DOC-SYNC 全 PASS；六份 cmp 为空 | 无 |
| DOC-502 | NOT_RUN | | |
| SCI-501 | NOT_RUN | | |
| SCI-502 | NOT_RUN | | |
| SCI-506 | NOT_RUN | | |
| SCI-503 | NOT_RUN | | |
| SCI-504 | NOT_RUN | | |
| SCI-505 | NOT_RUN | | |
| CONTRACT-501 | NOT_RUN | | |
| ARCH-501 | NOT_RUN | | |
| ARCH-502 | NOT_RUN | | |
| ARCH-503 | NOT_RUN | | |
| ARCH-504 | NOT_RUN | | |
| ARCH-505 | NOT_RUN | | |
| CLEAN-501 | NOT_RUN | | |
| GATE-501 | NOT_RUN | | |
| GATE-502 | NOT_RUN | | |
| PERF-501 | NOT_RUN | | |
| ACCEPT-501 | NOT_RUN | | |
| BLD-501 | NOT_RUN | | |
| E2E-501 | NOT_RUN | | |
| VIS-501 | NOT_RUN | | |
| REPORT-501 | NOT_RUN | | |

状态取值：NOT_RUN / RUNNING / PASS / PARTIAL / BLOCKED / NOT_RUN(负责人裁决)。

## 双线文件域冲突登记

| 时间 | 线 | 触及共享面 | 前台裁决 |
|---|---|---|---|

## 偏差与开放问题

- DOC-501：仓库 `artifacts/prerelease_v5/` 为未跟踪陈旧输出目录（D-14 同源），在本任务文件域外，登记转 GATE-501 清理。

## 发布门核对（收尾填写）

- [ ] 三篇论文式报告交付
- [ ] 五一致性独立验收通过
- [ ] fast + 全量 + Windows 全绿、零 waiver
- [ ] 两组成品帧 agent 自验通过、负责人目检认可
- [ ] OPEN_QUESTIONS 已交付
- [ ] FIN（README、0.0.1alpha）经负责人明确授权
