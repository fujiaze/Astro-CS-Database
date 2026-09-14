# 包取证摘要：AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【zip】工程控制/_control_packs/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828.zip
- 文件 37 | 97.7 KB
- sha256: 42a03c0b09168f3e865b05aa65578701beb74c268f0d52000cf3e4e82eecb7d8
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828
- zip 内 marker: 00_READ_FIRST.md, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_PRODUCT_ARCHITECTURE.md, 02_TASK_LEDGER.csv, 03_TASK_DETAILS.md, 04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md, 05_CPU_BACKEND_ABI_AND_PACKAGING.md, 06_BENCHMARK_AND_PROFILE_SPEC.md, 07_RESOURCE_MONITOR_AND_UTILIZATION_GATE.md, 08_SCIENCE_SYNTHETIC_AND_EXTERNAL_REVIEW.md, 09_LINUX_WINDOWS_BUILD_RELEASE.md, 10_GIT_REVIEW_CAPSULE_AUDIT_PACKAGE.md, 11_AGENTS_MD_REQUIRED_BLOCK.md, 12_V3_V4_MIGRATION.md, 13_ALPHA_VERSION_AND_PHASE3.md, 14_V4_COVERAGE_MATRIX.md, 15_CONTINUOUS_CHECKPOINTS.md
- zip 内台账: AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/02_TASK_LEDGER.csv 表头 ['task_id', 'phase', 'depends_on', 'scope', 'required_commit', 'required_push', 'preferred_host', 'status'] 行 98
- 00_READ_FIRST 开头:

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
2. 阅读本文件、`01_PRODUCT_ARCHITECTURE.md`、`02_TASK_LEDGER.csv`、`03_TA

