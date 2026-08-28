# 全局 Thread Budget 与执行架构 (V5)

> ID: ARCH-THREAD-001  状态: FROZEN (V5 ARCH-004, 2026-08-28)  上游: ARCH-002/ARCH-003  下游: BENCH-003(worker/block 候选)/BENCH-004(profile)/07 资源门
> 本文件为 V5 权威线程架构;旧 THREADING_MODEL.md 的分层与确定性锚点保留有效(§6 引用),冲突处以本文件为准。

## 1 全局 thread budget(单一来源)

- CLI 启动时建立**唯一**全局预算对象:`available_cpus = affinity ∩ cgroup ∩ Job Object`(非机器总核,与 ARCH-003 六查④同源);预算按 `phase→stage→kernel` 层级显式分配,任何时刻 Σ(活动 worker) ≤ budget。
- 分配策略(冻结): 串行 I/O 与控制面恒 1 线程;CPU 内核获得 `min(budget, kernel_block_hint)`;异步 I/O pipeline 恒 1 专用线程;后台服务(watchdog/资源监控/progress 日志)恒 1 线程+独立小预算(不入科学预算池)。
- **backend 禁私有线程池**(ARCH-003 §4);模块内 OpenMP 线程数经 host callback 注入运行时(由预算派生),禁 `omp_set_num_threads` 自定值;**全仓禁止硬编码线程数**(ARCH-001 清单 risk_note 列逐行登记)。

## 2 每阶段执行画像(串行 I/O · CPU task · async pipeline · backpressure)

| 阶段 | 串行 I/O | CPU task(并行粒度) | async pipeline | backpressure |
|---|---|---|---|---|
| Phase1 读入 | aio 顺序读(1 线程) | 校准/检测/PSF(逐帧行带) | 读→算双缓冲(深度=2) | 队列满时读阻塞 |
| Phase1 WCS/测光 | header KV 读写 | ipv 三角/投票(帧内) | — | 同步(见 §4) |
| Phase1 Drizzle/HiPS | tile 原子写 | overlap/accumulate(候选) / normalize(归并) | tile 写异步(深度=1) | 落盘完成才 release tile |
| Phase2 | UPM 模型读/写(串行) | sampler(串行 reference)→rejection(行带)→integration(行带) | — | 同步链 |
| Phase3 | HiPS tile 读(cache) | 反向映射+采样(行带) | tile cache 预取(深度=1) | cache 上界 O(cache_tiles·W²) |

- 每阶段在 run manifest 记录 `budget_alloc`(分配快照);07 资源监控以同对象为唯一事实来源。

## 3 异步与取消架构

- **async 仅两类**: I/O pipeline(读/写双缓冲)与后台服务;科学计算无 async/future(消除嵌套并行与不可预算并发)。
- **取消**: CLI JSONL cancel → 全局取消标志(原子)→ 各内核取消点(ALG 文档 5c 已逐内核冻结: 帧粒度/行带粒度/迭代间/整模型/整文件);取消后预算立即回收,取消单元不落盘(ARCH-002 §5)。
- **嵌套并行**: 禁止(科学内核不得在 parallel region 内再开并行;I/O 线程不得执行科学内核);唯一豁免=watchdog(独立预算)。

## 4 并发正确性合同(承接旧锚点)

- 浮点归约顺序冻结(旧 THREADING_MODEL.md §确定性锚点全部有效: upm.cpp:495/sampler 串行/drizzle_engine.cpp:1662,1751,1834,1843);tile 合并=thread-local 累加后 **t=1..num_threads 固定序串行合并**(与线程数无关的确定性: 结果序列由 budget 快照唯一化)。
- 计数器: atomic 或 thread-local 聚合;cache(UPM dense/Gaia/tile)线程安全或单线程互斥;无裸 data race。
- V5 修正: 旧文档"ACR work_pool+GPU 等价契约"与"浏览器层"**dormant/not-shipped**(ACR 不接入;browser 为 tool 分类)。

## 5 静态 checker 合同(验收)

`tools/arch/check_thread_budget.py`(BENCH-003 前落地,ARCH-004 先立合同):
1. 扫描 lib/ 生产源: `std::thread`/`std::async`/`_beginthread`/`CreateThread` 出现处必须在 `THREAD_BUDGET_EXEMPT` 登记表(当前: orchestrator watchdog/resource_monitor/logger;tests/ 全豁免);
2. `omp_set_num_threads(`/`num_threads(` 字面量=0 容忍;
3. 未登记即 FAIL(exit 1)——保证"未登记线程创建"机器可查。

## 6 关联

- 文档: THREADING_MODEL.md(分层+锚点)/EXECUTION_MODEL.md/ASYNC_IO_CONTRACT.md/OWNERSHIP_AND_LIFETIME.md/07_RESOURCE_MONITOR
- 任务: ARCH-004(本文件)/BENCH-003(候选不含硬编码 core count)/BENCH-004/ABI-001(host budget callback)
