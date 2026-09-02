# AstroCS V4：CPU 自适应预发布控制

## 目标

以当前 `origin/main` 为唯一开发对象，完成科学定义、算法、架构/API、代码和测试的正向统一；使用纯 CPU 自适应并行；最终只运行一次当前候选32R并生成 HiPS 审核产品。

## 本包优先级

本包完全替代 V3。V3 的历史 A/B/C、ACR GPU/Mixed、逐 Checkpoint 外审和历史32R任务全部取消。

## 硬规则

1. 只在 `main` 开发；每个修改任务一个原子 commit，测试通过后立即 push。
2. 不运行、不构建、不比较任何历史版本；数值正确性来自文档推导的合成 Oracle。
3. ACR 暂不接入生产；不得执行 GPU/Mixed 任务。保留模块源码但标记 dormant。
4. 禁止硬编码 CPU 核心数、线程数或全局 AVX/AVX2/AVX-512 编译选项。
5. 每个重计算 kernel 必须有安全标量参考路径；SIMD 变体只在编译器支持且运行时硬件/OS允许时注册。
6. 指令集和 worker 数由本机 benchmark 选择并缓存；缓存失效条件必须完整。
7. 所有预计或实际超过5秒的计算命令必须由资源监控包装器启动；没有 profile 证据等于未运行。
8. CPU 重计算阶段不得单线程或长期低利用率；I/O/元数据短串行允许，但必须与计算阶段分开计时。
9. Linux 负责静态/文档/合成小测/调度；Fatduck 在线时负责 Windows 编译、benchmark、真实数据和重计算。
10. Fatduck 离线时继续全部 Linux 可执行任务；禁止因 Windows 离线闲等。
11. 任务连续执行，不在中间等待外审；只有无法获得新权限、科学定义必须由用户裁决或最终发布包完成时才停止。
12. 最终审核包压缩后不超过25 MiB，单文件不超过5 MiB；禁止原始数据、完整 HiPS、长日志和像素 CSV。

## 启动顺序

1. 运行 `python3 scripts/validate_control.py .`。
2. 读取当前根 `memory.md`、相关模块 `memory.md`、`AGENTS.md` 和正式 docs。
3. 执行 `02_TASK_LEDGER.csv`，依赖满足后连续推进。
4. 失败时在当前任务范围内定位、修复、复测；不得用豁免把 FAIL 改 PASS。
5. 完成最后任务后输出 `AWAITING_EXTERNAL_RELEASE_REVIEW`。

## 审核责任

Agent 负责收集代码事实、起草文档、实现和机器验证；核心 SCI/ALG 推导、文献依据及科学一致性的最终核实由外部审核人完成。每个 commit 生成审查胶囊，但不中断独立任务。最终发布不得只依据 Agent 自报 PASS。

## 唯一允许状态

`NOT_STARTED / IN_PROGRESS / PASS / FAIL / BLOCKED / DEFERRED / REVIEW_PENDING`

`DEFERRED` 仅用于本控制明确排除的 ACR GPU/Mixed 和历史版本任务，不得用于隐藏当前范围缺陷。
