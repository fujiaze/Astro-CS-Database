# 覆盖率基线（首次 deep CI，V8-CI-005）

- status: ACTIVE
- owner: SA-CI-32
- source_task: V8-CI-005（控制包 V8.1，07_CI_MACHINE_CONTRACT.md）
- created_utc: 2026-09-05
- scope: linux-deep profile（DEEP-COV 检查）

## 合同约束

`docs/standards/07_CI_MACHINE_CONTRACT.md` 规定：**第一次 deep CI 只测量并
记录覆盖基线，不虚构覆盖率阈值**。本文件即该基线的登记处；覆盖率阈值
（per-module `--cov-fail-under` 或全局 gate）冻结由后续质量任务完成，冻结前
`tools/quality/ci_coverage_runner.py` 的 `threshold` 字段保持 `null`。

## 基线获取方式

- 检查入口：`ci/run.py --profile linux-deep`（DEEP-COV，见 `ci/checks.json`）。
- 驱动：`tools/quality/ci_coverage_runner.py`（pytest-cov 薄封装，
  `--cov=lib --cov=cli --cov=tools --cov-branch`，分支覆盖）。
- 产物：`run/ci/coverage/coverage.xml`、`coverage.json`、
  `coverage-summary.json`（含 pytest 退出码透传）。
- 环境前提：pytest + pytest-cov（CI 环境 ubuntu-24.04 提供）。本地容器缺
  pytest 时该检查按 `prerequisite_tools=["pytest"]` 判 SKIPPED(waivable)。

## 基线记录

首次 hosted 运行后在此追加：测量 UTC、运行器镜像、commit SHA、
各包（lib / cli / tools）行覆盖与分支覆盖数值、报告文件登记到
`evidence/`（CI 产物目录），并保持 `threshold: null` 直至阈值冻结任务落地。

> 冻结阈值前禁止在 CI 或文档中声称任何"覆盖率达标"结论。
