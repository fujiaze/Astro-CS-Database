# 05｜工程标准化：旧高性能算法进入V2唯一Host/Task/Pipe/Storage

**这是工程替换计划，不是再造数学模块。** 正式V2骨架由 `SYSTEM.md`、`API.md`、`OBJECTS.md`、`EXECUTION.md`拥有；唯一可编译接口由现有 `include/acsd/runtime/*.hpp` / `include/acsd/modules/*.hpp` 拥有。本文只给出怎样移植main生产调度、I/O/并行优化，哪些旧机制必须退休以及必须保持的性能不变量。

## R-00 先按真实消费者重连，不把1.8万行旧adapter搬来

main `lib/infrastructure/cli/commands.cpp`→`lib/infrastructure/scheduler/src/{runtime,scheduler,module_adapters,block_frame,block_flow,artifact_store}.cpp` 已有真实功能，但 `module_adapters.cpp` 约18577行混合JSON读取、科学参数解析、老C ABI、工作目录、计数器、内存配置和模块调用。旧 `p1_*.json` / `p2_*.bin` 不是正式必需的物理数据域。

**V2施工单**：
1. 将旧每个 `p1_op_* / p2_op_* / p3_op_*` 的解析分成 **(i) typed request binding, (ii) 可复用纯数值kernel, (iii) publish/diagnostics**，接入现有22 Process和固定三命令workflow。每个操作只能有一个生产代码入口；旧session C ABI compatibility与旧per-node目录不能成为第二套执行线。
2. 保留旧算法调用时真正检查过的输入shape/单位/WCS/response和失败状态，现由V2 `Result<>,Assessment,Issue`表达，不能把旧全0 sentinel/空文件当成功。迁入main行为时附严格表：旧flag→V2正式参数→实际消费者→无效时Code→单位/identity。
3. 大像元数组作为Ref/ArrayRef + Storage Block由运行期维护不可变ID/只读lease；必要科学元数据为小的typed对象，不能嵌入传输全4096² JSON。真正的下游根据持久化对象引用取块，不能“名字存在就算上游完成”。
4. 迁移后按实际硬依赖删除旧临时工作文件消费者；必要的外部/FITS交换仅在阶段边界和受控spill/cache，三命令在新进程上读取正式manifest。检验无任何读者把兼容sid ecar当唯一真值。
5. Runtime/process的缓存键与科学身份分离：`input_sha + calibration_model + WCS/SIP + response/PSF + GeometryRecipe/order + root identities + source_sha`；科学不变的下游共享一个不可变输出。仅sky/weights变化不能重新Normalize；仅目标Export WCS变化不能重新Mosaic；改变自动order必须让R→产品→Mosaic→Export失效，而非错误重用旧order13产品。

## R-01 BlockFrame/Task/Pipe和唯一资源预算

**main既有**：`lib/infrastructure/scheduler/src/{block_frame,block_flow,executor,scheduler,plan_estimator,memory_budget,check point,cpu_budget}.cpp`。main `scheduler.cpp` 支持DAG依赖、失败/取消传播、内存回压；`executor.cpp` 有CPU共享池、有限I/O队列、ThreadBudget lease。V2已经有 `src/runtime/{host,pool,task,pipe,memory,storage}.cpp`；不把main旧ThreadBudget/Scheduler作为第二生产线程池。

任务：
- 以V2现有`ProcessHost::invoke`的scope/Task生命周期管理多帧与块；从main移植已检验的**并行轴计划、确定性归约、估计工作单元与取消安全点**，而不是整套线程类。
- 每个块在正式运行前计划`memory_bytes, cpu_tokens, io_slots, spill_bytes, work_units`，全链只从同一MemoryScope预留/借用/归还；父scope不能因子模块设置更高限额被绕过。第三方CfITSIO/MPFR/GAIA解析、vector reserve、缓存和header parser也要计实际峰值（不一定都能精确预留，需安全余量与实测）。
- 对大对象固定任务生命周期：reader→calibrator→PSF/Drizzle→spill→publish；每段同样科学InputRef，下游只读Buffer不复制大Array；用有界Pipe的背压避免“消费者慢而生产者预计算196帧大图”。
- 一个数学kernel的内核工作循环可多线程，**不能**每个Process再创建CPU池；V2执行器取得唯一budget/TaskLease，旧临时并行需要改为任务分区/线程无状态并行或受控kernel局部并行，杜绝双层 N×N oversubscription。
- Process编码规定输入/输出对象hash、unit/index、支持/unknown，唯一出错路径通过host向CLI非零退出；向Host公开`needed_blocks/dependencies`供缓存与重算，而不是单次把所有中间数据物化。
- 中止/资源紧张时要得到可复现的`cancelled/resource_exhausted`并原子不发布，已成功的上游块可断点重用；失败/取消所有在途worker被唤醒，不能留下等待bounded队列的死锁。

## R-02 旧P1帧间/帧内并行优化必须移植语义

main `module_adapters.cpp::p1_parallel_for` 处理多个frame worker与单frame内OpenMP预算；生产历史曾有`omp_set_num_threads`的ICV线程局部遗留、造成后续IPV/Drizzle只有1线程；反向错误是frame轴N个任务各启动N个OMP线程池、带N倍临时缓冲。旧代码解决了一部分预算问题，V2不能复发。

- 并行轴规划：给定Nframes、CPU预算P、每帧独立work估计和常驻RAM，决定`frame_workers F`与`inner_workers I`并满足`Σactive_cpu≤P`，内层math kernel与外层帧轴**一份token账本**；不得简单设置F=P,I=P。
- 复用旧合适的stripe/region任务计划，线程局部候选/SIP/PSF/Overlap scratch在任务结束释放；优先使CPU处理计算而非每worker重复读Gaia、response、Flat、几何cache。
- 避免用全局`OMP_NUM_THREADS`来猜V2线程配额：旧Sky和部分IPV会读环境/当前ICV。若暂时调用旧OpenMP计算核，应在进入前用小的RAII线程预算/还原和隔离，不与共享Runtime冲突；最终由host决定并行边界、避免热kernel自行创建长生命周期池。
- 固定同输入worker=1/2/4/8的数值等价与真实`in_flight`、CPU utilization、peakRSS，区分“命令行给8核”与“真正同时工作8核”。不能拿多线程浮点随机差异作为减精度理由。
- 动态自动NSIDE后工作量激增，应根据有效源像元/候选叶/CSR nnz估算F与I并用窄块限制峰值，不在高阶多帧过程中把每帧全4096² R全表同时留内存。

## R-03 旧内存与I/O性能优化的统一承接表

| main已有有效结构 | 旧位置 | V2唯一承接位置 | 不可丢的性能不变量 |
|---|---|---|---|
| touched叶/tile稀疏写 | `drizzle_engine.cpp` + `astro_sphere_sink.cpp` | `geometry.drizzle` + Product/Storage | 无支持tile不物化，不因高nside构造12*nside²空表 |
| 每线程bounded target-ipix几何LRU + run_generation | `drizzle_engine.cpp` | Geometry纯核+Host配置的缓存服务 | 不跨order/WCS错误复用缓存，少算重复叶边界 |
| stripe唯一作者+固定归并 | `drizzle_engine.cpp` | Drizzle任务划分/确定性归约 | 避免共享叶浮点atomic；线程数变化不更改科学结果 |
| per tile读一次、局部采样 | `coverage/src/sampler.cpp` | sky.controls+Storage ReadBlocks | 同一候选tile不被每一个control重新打开 |
| sky Schur/IRLS、block Cholesky | `coverage/src/sky_plane.cpp` | sky.fit + V2 Numerics/CPU租约 | 只装必要稀疏矩阵，小系统串行、大系统分块 |
| 动态microchunk内存守卫 | `coverage/src/block.cpp` | MemoryScope plan + Region Pipe | 预算不够缩块/Spill；固定开销超限拒绝，不能冒险OOM |
| header-only BoundedAsyncQueue背压 | `coverage/include/astro/phase2/async_io.h` | `src/runtime/pipe.cpp` | bounded、close、cancel、error propagation，不能复制第二队列 |
| reader/compute/writer三段Export子块 | `scheduler/src/export_stream.cpp` | V2 Task/Pipe+`fits_writer` | CPU/IO可重叠，实际队列字节有界，失败不留完整FITS |
| 共享tile LRU与负缓存 | `resample/p3_resample.cpp` | Storage tile cache / projection.export | cache上界不随worker数线性涨；失败tile不重复IO |
| 完整原子发布/严格FITS校验 | `fits_output/p3_output.cpp` | Product/Storage/FITS Writer | staging验证、flush/fsync/rename、new-process reopen |
| 旧CPU租约+profile | `scheduler/src/{cpu_budget,plan_estimator}.cpp` | V2 Services/Memory/Probe | 不双重定义预算，不绕开进程有效CPU/IO限制 |

main某个功能在代码里存在不等于它有证据正常工作；每行先由执行Agent核实旧生产实际调用，再作性能行为移植。抽旧算法前先测源函数依赖与编译闭包，避免C ABI/静态全局副作用被偷偷带入V2。

## R-04 存储、闭包和不重复落盘

- V2 `src/runtime/storage.cpp`已有 canonical block hashing、bytes/metadata身份、异步写与CPU任务分离；`src/workflows/product.cpp`完成正式HiPS/FITS产品聚合。**只此一套**拥有科学数据块/发布状态。旧AIO算法可仅复用FITS解析/压缩/Tile几何和文件操作的经证内容。
- 发布前做完整Graph闭包：action孩子/CSRpayload、所有support/signal/units/WCS/response/sky/weights/shared noise、source+recipe hashes，全依赖可达；在全新Host无缓存的进程中至少一次重开。损坏一个LinearAction、manifest、header、hash必须fail-closed。
- `run/`是唯一任务私有可写scratch; `testdata/`和`gaia/`原始只读。空间规划为`raw引用 + 有效Normalize单帧缓存 + 被引用的Mosaic分块 + Export final + staging + safety margin`，不是每个修复版本都复制196帧与输出。
- 已成功的calibration/Gaia/WCS/response/PSF/R/Normalized product都按科学模型身份复用。失败staging和无引用缓存才可删除；未知路径、其它项目、已发布final不能因空间紧张被清理。
- 日志包含`source_sha/input_set_hash/recipe_hash/output_hash/reused/invalidated/reran/RSS_peak/disk_peak/IObytes/actual_cpus/cleanup`，大图二进制不进Git，短JSON/数学反例和独立QA报告进Git。禁止每次从头计算校准并重新SHA所有196帧。

## R-05 CLI、GUI与双平台

- main的唯一CLI协议是`acsd normalize|mosaic|export --json` + JSONL event/status/exit code，迁入V2现有`apps/cli/{commands,config,events}.cpp`；旧配置字段可在重构期不兼容，但用户可达的**功能能力**必须在新配置中有明确等价入口。不能新系统内部实现autoNSIDE而CLI只能固定order。
- main `infrastructure/hips_browser` Qt/OpenGL与GUI产品接线可在正式科学链收敛后迁到V2新的GUI服务边界；不是迁整个旧界面状态机到headless Runtime。把旧界面做为功能候选清单，后续由独立任务评估。
- Windows amd64/MSVC、Linux amd64/GCC/Clang：同一科学输入和合法WCS，在真实目标域运行；检查FITS外部库同步、大小端/索引、长路径、线程ICV及错误传播。单次Linux成功不能把双平台标完成。

## R-06 本组完成标准

源码与调用关系可画出：一个public command→一个V2 Workflow→一套typed operations→一套Task/Pipe/Storage→一个提交的product closure；没有并行保留旧science Runtime或重复IO后端。CPU/内存/磁盘模型经实际profile可解释，线程/缓存/Spill工作且不破坏数学；取消、写盘失败、超预算后不发布伪成功，旧缓存复用真实可观测；性能门见[06](06-PERFORMANCE.md)。
