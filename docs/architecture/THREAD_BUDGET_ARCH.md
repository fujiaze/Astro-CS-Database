# 全局 Thread Budget 与执行架构 (V5)

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> ID: ARCH-THREAD-001  状态: FROZEN  上游: ARCH-002/ARCH-003  下游: BENCH-003(worker/block 候选)/BENCH-004(profile)/07 资源门
> 本文件为线程架构权威;THREADING_MODEL.md 的分层与确定性锚点保留有效(§6 引用),冲突处以本文件为准。

## 1 全局 thread budget(单一来源)

- CLI 启动时建立**唯一**全局预算对象:`available_cpus = affinity ∩ cgroup ∩ Job Object`(非机器总核,与 ARCH-003 六查④同源);预算按 `phase→stage→kernel` 层级显式分配,任何时刻 Σ(活动 worker) ≤ budget。
- 分配策略(冻结): 串行 I/O 与控制面恒 1 线程;CPU 内核获得 `min(budget, kernel_block_hint)`;异步 I/O pipeline 恒 1 专用线程;后台服务(watchdog/资源监控/progress 日志)恒 1 线程+独立小预算(不入科学预算池)。
- **backend 线程池经 host callback 注入**(ARCH-003 §4);模块内 OpenMP 线程数经 host callback 注入运行时(由预算派生),取值与 `omp_set_num_threads` 无关;**全仓线程数由预算派生**(ARCH-001 清单 risk_note 列逐行登记)。

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
- **嵌套并行**: 外层已并行则内层串行(科学内核只在 parallel region 外开并行;I/O 线程与科学内核分属不同线程);唯一豁免=watchdog(独立预算)。

## 4 并发正确性合同(承接旧锚点)

- 浮点归约顺序冻结(THREADING_MODEL.md §确定性锚点全部有效: coverage/src/upm.cpp:605/sampler.cpp:924-954 固定槽位/drizzle_engine.cpp:1923,2117-2142,2279);tile 合并=**per-stripe scratch pool 累加 + 按 stripe 索引升序左折叠归约**(累加与归约解耦, drizzle_engine.cpp:1885-1891 pendingStripe/merge_cursor、归约分支 :2117-2142; 与 P15a 左折叠完全同序 ⇒ 浮点结合树逐位一致, **与线程数/调度顺序无关**, 结果序列由 budget 快照唯一化)。注: :2270 现为 prof 计数器合并, 非浮点 tile 合并锚。
- 计数器: atomic 或 thread-local 聚合;cache(UPM dense/Gaia/tile)线程安全或单线程互斥;无裸 data race。
- ACR 与浏览器层**dormant/not-shipped**(ACR 不接入;browser 为 tool 分类)。

## 5 静态 checker 合同(验收)

`eng/tools/arch/check_thread_budget.py`(BENCH-003 前落地,ARCH-004 先立合同):
1. 扫描 lib/ 生产源: `std::thread`/`std::async`/`_beginthread`/`CreateThread` 出现处必须在 `THREAD_BUDGET_EXEMPT` 登记表(登记表以 checker 内 THREAD_BUDGET_EXEMPT 为唯一事实源, 随实现演进, 以实跑输出为准; eng/tests/ 为扫描面豁免[测试面豁免=20], 不占登记条目)。当前生产源码面登记实况(实跑汇总=25 键/41 命中), 生产科学模块面核心六条: upm.cpp per-call 池 ×5(:620/:751/:794/:916/:2143, cworkers = Runtime lease) + sampler.cpp:934 per-call 池(workers = Runtime lease); 另有 weight_chain_selfcheck.cpp:479(权重链独立 Oracle 自查池, 不在根构建图内)、cosmetic/module_entry.cpp:64(omp_set_num_threads 租约注入)、历史保留面 orchestrator watchdog 路径级登记(orchestrator.cpp:5201/:5208)与 orchestrator.h:424/resource_monitor.h:135 文件级豁免等;
2. `omp_set_num_threads(`/`num_threads(` 字面量=0 容忍;
3. 未登记即 FAIL(exit 1)——保证"未登记线程创建"机器可查。

## 6 关联

- 文档: THREADING_MODEL.md(分层+锚点)/EXECUTION_MODEL.md/ASYNC_IO_CONTRACT.md/OWNERSHIP_AND_LIFETIME.md/observability/RESOURCE_MONITORING_CONTRACT.md
- 任务: ARCH-004(本文件)/BENCH-003(候选不含硬编码 core count)/BENCH-004/ABI-001(host budget callback)
