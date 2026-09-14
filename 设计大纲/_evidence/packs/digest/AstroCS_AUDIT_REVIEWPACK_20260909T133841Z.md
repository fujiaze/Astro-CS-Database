# 包取证摘要：AstroCS_AUDIT_REVIEWPACK_20260909T133841Z

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【zip】artifacts/AstroCS_AUDIT_REVIEWPACK_20260909T133841Z.zip
- 文件 14556 | 200535.0 KB
- sha256: 3b5d9c230473e4210bac0e1e636cfef317df524307e908ce0f1f17940c16ab2c
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: .clang-format, .editorconfig, .gitattributes, .github, .gitignore, AGENTS.md
- zip 内 marker: 00_READ_FIRST.md, MANIFEST.json, 00_READ_FIRST.md, START_PROMPT.txt, AUTONOMOUS_ENTRY.md, 00_READ_FIRST.md, 00_READ_FIRST.md, MANIFEST.json
- zip 内台账: artifacts/prerelease_v5/AUDIT_REVIEW/TASK_LEDGER.csv 表头 ['task_id', 'phase', 'depends_on', 'scope', 'required_commit', 'required_push', 'preferred_host', 'status'] 行 98
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

