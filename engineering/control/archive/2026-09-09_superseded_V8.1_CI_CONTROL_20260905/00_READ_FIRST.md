# 00｜唯一执行入口

## 目标

在服务器上已经存在且正在使用的 AstroCS `main` 工作区内原地继续。不得创建、克隆、移动或替换仓库。先冻结现有状态，再接入 GitHub 托管 Linux/Windows CI，修复已确认的架构、科学、文档和 CPU 并行问题；Fatduck 只用 CI 产出的 Windows 候选程序运行本地真实数据终验。

## 先读顺序

1. `01_FROZEN_CONSTRAINTS.md`
2. `02_CURRENT_STATE_AUDIT.md`
3. `03_VM_AND_CI_ARCHITECTURE.md`
4. `04_FOREGROUND_AGENT_RUNBOOK.md`
5. `CONTROL_TASK_LEDGER.csv`
6. 当前任务对应的 `tasks/*.md`
7. `GATE_REQUIREMENTS.csv`

## 不得违反

- 从当前目录运行 `git rev-parse --show-toplevel` 获取现有仓库根目录；禁止假定固定绝对路径。
- 只在现有 `main` 开发；禁止新分支、`git worktree`、额外 clone、仓库搬迁、目录重新规划和新建开发账户。
- 首先记录 `HEAD/main/origin/main`、remote、完整 `git status --porcelain=v2` 和现有任务证据。不得自动 `reset`、`stash`、`clean`、`rebase` 或删除现有文件。
- 前台 Agent 只分发、机器验收、提交、push、状态汇总和打包；固定子 Agent 执行任务。
- 只读任务可以并行；同一现有工作区的 tracked 文件写入必须串行。
- 一个任务一个原子 commit，直接 push `main`；禁止 amend、force push 和历史重写。
- 机器检查自动推进。除缺少权限/凭据、远端分叉无法快进、Fatduck 离线及最终发布裁定外，不等待用户签字。
- 不做历史版本全链路重算。数值正确性使用科学文档推导的合成 Oracle、解析解、守恒量和不变量。
- 当前生产只使用纯 CPU；ACR 保留但不可进入生产路由。
- heavy 计算必须动态并行，并采集 CPU、线程、内存、IO、进度和 worker 负载；单线程或持续低利用率直接失败。
- 不硬编码核心数、线程数或 SIMD 路径；benchmark 生成主机配置。没有配置时使用保守通用 amd64 路径。
- Phase1、Phase2、Phase3 独立调用、独立恢复、独立验收，不强制串行执行。

## 包自检

在控制包目录执行：

```bash
python3 validators/validate_control.py --root .
python3 validators/selftest.py --root .
```

自检通过后执行 `V81-ADOPT-001`。控制包不得被解压覆盖项目源码；其位置沿用项目现有控制目录惯例。若尚无惯例，仅在仓库内建立一个明确的活动控制目录，不移动任何既有内容。

## 完成定义

1. `G-ADOPT`、`G-CI`、`G-FIX`、`G-FAT` 自动证据满足；
2. GitHub Linux/Windows 在同一提交 SHA 通过；
3. 当前工作区原有修改和任务状态均已登记、归属清楚且未被覆盖；
4. 科学定义—算法—接口—函数—源码—测试追踪成立；
5. Phase2 等 heavy 路径不存在伪并行、单线程生产实现或持续低利用率；
6. Fatduck 使用最新候选完成真实数据终验，原始数据不外传；
7. 审核包通过白名单、哈希、大小和必需文件校验；
8. 最终发布仍由 Owner 根据报告与 JPG/本地结果裁定。
