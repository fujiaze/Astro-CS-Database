# AstroCS V5 预发布控制包：单一 CLI / amd64 CPU 自适应

## 0. 本包的唯一目标

在当前 `main` 上收敛出可发布候选：科学定义、算法、架构、接口、实现和测试可追溯且一致；Windows/Linux 只向用户暴露一个 CLI；重计算由纯 CPU 多后端按逐内核 benchmark 自适应选择；任何长时间低利用率、内存异常增长或静默串行均不得通过。

本包取代 V3/V4 的未完成门禁。历史报告仅作为问题线索，历史 PASS、历史数值输出和历史性能结果均不是当前证据。禁止为等价性反复运行历史版本。

## 1. 不可解释、不可放宽的硬约束

1. 只在 `main` 开发；不得创建分支。每个 Task 一个原子 commit，成功后立即 push。
2. 仅支持 `amd64/x86-64`；不得扩大到 ARM、NEON、SVE 或 32 位。
3. 发布包每个平台只暴露一个入口：Windows `astrocs.exe`，Linux `astrocs`。
4. Phase1、Phase2、Phase3 必须由该 CLI 的稳定命令和内部接口调用；发布包不得携带旧 Phase 可执行程序。
5. 当前不得接入 ACR/GPU/Mixed。ACR 可保留为 dormant 代码，但生产路由、测试和默认配置不得触达它。
6. 重计算必须并行且自动采集 CPU、内存、I/O、线程和阶段信息。低利用率不是 WARN，是 FAIL。
7. 禁止硬编码 CPU 核数、worker 数、AVX 路径或全局 `-march=native/-mavx*`。
8. ISA、workers、block/chunk 均由硬件安全检测与逐内核 benchmark 选择。没有可用 profile 时必须走保守 amd64 baseline，但仍按进程 affinity 动态多线程。
9. 数值验证由文档推导的独立合成 Oracle 完成；容差必须在看到结果前冻结。
10. Windows 最终 32R 真实数据只跑当前候选一次；不得跑历史版本，不得用旧输出冒充。
11. 除最终发布包外，不设等待外部批准的停止点。Fatduck 离线时继续 Linux 可执行任务。
12. Task 状态只能来自 `02_TASK_LEDGER.csv`；叙述性报告不得把 PARTIAL/WARN/未运行改写为 PASS。

## 2. 首次启动后 30 分钟内

依次执行，不得先改代码：

1. 解包到仓库外，运行 `python3 scripts/validate_control.py .`，必须输出 `CONTROL_PASS`。
2. 阅读本文件、`01_PRODUCT_ARCHITECTURE.md`、`02_TASK_LEDGER.csv`、`03_TASK_DETAILS.md`。
3. 阅读仓库根 `AGENTS.md`、`memory.md` 和相关模块 `memory.md`；不得让过长历史 memory 替代当前源码核实。
4. `git fetch origin --prune`，记录 `HEAD/main/origin/main/status/remote`；remote 输出必须脱敏。
5. 将 `11_AGENTS_MD_REQUIRED_BLOCK.md` 的短块合并到根 `AGENTS.md`。
6. 复制模板为工作证据表，Task 只能按依赖顺序认领；同一时间只能有一个 Task 为 `IN_PROGRESS`。

## 3. 连续执行状态机

`NOT_STARTED -> IN_PROGRESS -> PASS | FAIL | BLOCKED | REVIEW_PENDING`

- `PASS`：全部验收项、测试、证据、commit、push 均完成。
- `FAIL`：实现或验证不满足门禁；立即修复，不能跳过。
- `BLOCKED`：仅限真实外部依赖。必须记录阻塞对象、实测命令、时间和不受阻的下一 Task。
- `REVIEW_PENDING`：仅用于已提交的审阅胶囊，Agent 必须继续其他无依赖 Task，不得停工。
- 禁止 `DEFERRED` 用来绕过预发布必需项。

## 4. 唯一允许停止的条件

- 权限/凭据缺失且无法继续任何独立任务；
- `main` 出现无法归属的外部修改，与当前 Task 文件重叠；
- 数据损坏、仓库身份不一致；
- 最终 `REL-003` 已完成并生成 `AWAITING_EXTERNAL_RELEASE_REVIEW` 审核包。

Fatduck 离线、Linux 算力低、某个非关键工具缺失都不是整体停止理由。

## 5. 最终 PASS 含义

Agent 无权宣布发布。全部 Task 通过后只能写：

`AWAITING_EXTERNAL_RELEASE_REVIEW`

最终由用户与独立审核者核查科学文档、逐层追溯、抽样源码、Windows 32R HiPS/接缝和资源曲线后放行。

