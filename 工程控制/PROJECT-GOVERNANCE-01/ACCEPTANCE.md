# 验收记录（ACCEPTANCE）

> 本包尚未执行。`PASS` 只可由前台（调度员）独立复跑后填写（见 `OPERATOR.md §3`）。
> 填写要求：证据路径必须指向本次复跑产生的日志或报告；不得复用执行 Agent 的自述。

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| BASE-001 | NOT_STARTED | — | — | — |
| GOV-001 | NOT_STARTED | — | — | — |
| DOC-001 | NOT_STARTED | — | — | — |
| DATA-001 | NOT_STARTED | — | — | — |
| CFG-001 | NOT_STARTED | — | — | — |
| MOD-001 | NOT_STARTED | — | — | — |
| ARCH-001 | NOT_STARTED | — | — | — |
| AIO-001 | NOT_STARTED | — | — | — |
| RT-001 | NOT_STARTED | — | — | — |
| CLI-001 | NOT_STARTED | — | — | — |
| CLI-002 | NOT_STARTED | — | — | — |
| CLI-003 | NOT_STARTED | — | — | — |
| P1-001 | NOT_STARTED | — | — | — |
| P1-002 | NOT_STARTED | — | — | — |
| P2-001 | NOT_STARTED | — | — | — |
| P2-002 | NOT_STARTED | — | — | — |
| P3-001 | NOT_STARTED | — | — | — |
| P3-002 | NOT_STARTED | — | — | — |
| CPU-001 | NOT_STARTED | — | — | — |
| OBS-001 | NOT_STARTED | — | — | — |
| INT-001 | NOT_STARTED | — | — | — |
| CI-001 | NOT_STARTED | — | — | — |
| QA-001 | NOT_STARTED | — | — | — |
| PKG-001 | NOT_STARTED | — | — | — |
| REAL-001 | NOT_STARTED | — | — | — |
| FINAL-001 | NOT_STARTED | — | — | — |

## 控制包级门

- [ ] 所有任务 PASS（且每条 PASS 都有前台独立复跑证据）；
- [ ] GitHub Linux/Windows CI 在同一 SHA 全绿；
- [ ] Linux 最终 SHA 真实数据流通过；
- [ ] 图像审核（Agent 初审）通过，Owner 终审留白；
- [ ] Windows/Fatduck 同 SHA 复验通过（未完成则如实记 AWAITING_WINDOWS_VALIDATION）；
- [ ] FINAL-001 汇总经负责人审阅；
- [ ] 无 FAIL / BLOCKED / UNRESOLVED 冒充完成。

## 登记模板（复制到对应行）

```text
状态：PASS / FAIL / BLOCKED
机器门结果：<逐门一句话 + rc>
证据路径：run/PROJECT-GOVERNANCE-01/<TASK>/logs/…
前台结论：<复跑是否与执行 Agent 自述一致；有无越界；遗留项>
```
