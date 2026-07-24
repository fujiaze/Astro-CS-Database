# 自治独立复核协议

## 目的

在没有用户逐项审批的情况下，仍然防止实现 Agent 自证成功。

## 输入

- 当前任务规范与验收条件；
- 任务开始前基线；
- Git diff 或文件变更清单；
- `TASK_REPORT.md`；
- `TEST_REPORT.md`；
- `EVIDENCE_INDEX.md`；
- 原始日志、输出和数据哈希。

## 复核步骤

1. 核对任务范围，找出越界修改；
2. 核对入口条件和原始失败/基线证据；
3. 检查实现是否真正解决任务目标，而非仅改变表象；
4. 检查测试是否覆盖验收条件、失败路径和兼容性；
5. 抽查至少一个关键测试或重新运行关键测试；
6. 检查数据、ABI、CLI、配置、schema、依赖和文档是否同步；
7. 检查超时、错误处理、日志和回退；
8. 检查用户原有改动是否被误改；
9. 检查证据可否在另一份 clone 或同等环境重现；
10. 给出 PASS、FAIL 或 BLOCKED。

## REVIEW_REPORT.md 最小结构

```text
# Review Report
Task: <task_id>
Reviewer mode: sub-agent | isolated-self-review
Baseline: <commit/hash>

## Scope review
## Acceptance review
## Test and evidence review
## Compatibility review
## Risks and residual issues
## Required corrections

VERDICT: PASS | FAIL | BLOCKED
```

只有 `VERDICT: PASS` 才允许把任务置为 DONE。
