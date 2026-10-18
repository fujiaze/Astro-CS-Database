# ARS-01 / S0：旧 Runtime 生产调用链、科学输入与资源基线

Issue [#23](https://github.com/fujiaze/Astro-CS-Database/issues/23)，上位 Issue #22。核查起点：远端 main SHA 476c023d395b224518aa1831f730d64822250289。本文件是**源码静态审查 + 历史证据重新定位**，不是当前 main 的三阶段实测。**W1/W2/W3 当前版本均 NOT_RUN；ARS-01 验收未完成，#24 不得依据本报告自动启动。**

## 一、真实证据与历史输入

- 必读及已核对：AGENTS.md、docs/ACSD_DESIGN.md、docs/engineering/architecture/PROCESS_RUNTIME_ARS.md、PROCESS_RUNTIME_IMPLEMENTATION.md、工程 Runtime/Block/AIO/Scheduler 合同；实际 CLI command_tree.h、parser.cpp、commands.cpp、runtime_client.cpp；scheduler/runtime/pipeline/module_adapters/memory_pressure/block_flow、核心 executor/BlockFrame、AIO、三阶段 session、benchmark 及有关科学与实验记录。目标 ARS 文件只是设计，不是生产实现。
- **W1 权威成功样本**：实验/issue12-simpson-domain/evidence/50-cfg6.json，实验/issue92-wcs-gaia-match/evidence/00-identity.json、41-cli-fix.json、REPORT-A1-FIX.md。M42 T3/M1 Red 六帧（20251126/27/28、20251211/12/13）加 T3 bias/dark/Red flat 三主帧，九文件各有 SHA-256/bytes。GaiaDR3SP 有 20 个 XPSD 文件共 67,834,893,663 B，**仅文件名和字节数**，未提供全量内容 SHA。真实原始 FITS/XISF/XPSD 均在运行机 testdata/gaia，Git 树不含这些原始观测文件。
- #92 的历史 E2E 使用成功修复代码 5169ccc9d957165ce98a218b26c3b7a169a5dbd4，binary SHA-256 为 5277fcb895df802d4fd46468453208d3d2ee5a8e37362d06e5ee7958f15d8aa4；**历史**生产 exit=0、6/6 applied，Gaia 入帧1083–1090，IRLS 929–972，首帧 k_photo=6.222285086050469e-17，历史资源记录 wall=566.5s、等效核5.61、RSS峰值12.66GB、I/O读取4.89GB/写入6.58GB、线程峰值78。**这些全部是 #92 旧 SHA/旧 binary 的历史值，不是本次测量，也不能复制进当前测量列。**
- 实验/m42-realdata/README.md 与 docs/DATA-SOURCES.md 对应另外一组 49 帧 RELEASE-05；其 P2/P3 产品曾有 covered_but_nonfinite 问题/verdict FAIL，不能当作本轮 W2/W3 合法成功基线。
- Issue #91 最近 E1-R 修复附带 Stage3 worker-open 失败注入 A/B，不能替代相同当前 binary 和合法同阶 P2→P3 正常工作负载。#87 尚 Open；损坏 tile 被当作合法空足迹的问题不能默默归为正常输出。#9 的光度后 BUNIT/方差语义仍未独立闭合，W1 仅能作带身份的代码行为回归。

## 二、生产调用链（STATIC_CONFIRMED）

统一 CLI 命令为 acsd normalize / mosaic / export --json CONFIG -y，而不是早期用户命令 phase1/phase2/phase3。

1. lib/infrastructure/cli/commands.cpp 的 run_phase1_block / run_phase2_block / run_phase3_block（其中 Phase1 原始命令体的详细入口同文件）进入 run_with_resource_gate；以独立监控线程和 ResourceRecorder 采样，并解析 CPU profile、可用内存、执行 backend。
2. lib/infrastructure/cli/runtime_client.cpp::run_pipeline 调用 register_cli_modules(reg) / build_pipeline_ir / create_runtime(rrb) / load_pipeline / Runtime::run(ctx)。CLI adapter 显式绑定 IR artifact 依赖，节点运行后的 trace 与 node manifests 被收集。
3. lib/infrastructure/scheduler/src/runtime.cpp 的 RuntimeImpl::load_pipeline 用 PipelineIRParser 验证与建依赖图；每节点由 registry_.create(n.module_id) 取得 C++ IModule 工厂，module.plan 和真实 execute 由 Scheduler 调用。lib/infrastructure/scheduler/src/module_adapters.cpp::register_phase_modules 实际注册 8+7+5 个内建工厂。**这不是通过 acsd_module_query_v2 动态 DLL 执行科学 operation 的路径。**
4. AIO 是生产文件读/写实现边界，阶段产品按历史协议经过 FITS/JSON/bin 物化与复读。Frame/typed Block 的目标生命周期与这些真正落盘的计算依赖不可混称。

| 阶段 | 真正 IR 节点 | 当前实际工作方式 | 实测 |
|---|---|---|---|
| W1 Normalize | cal→cos→wcs→psf→phot→snr→drz→wr（8） | P1NodeOp 委托实际内核 ac_calibrate_frame、ac_correct_frame 和 ac_correct_columns_ex2，IPV 解算、PSF、Gaia 拟合、SNR、Drizzle、HiPS writer；工作区 calibrated_*.fits、cleaned_*.fits、p1_wcs.json、p1_phot.json、p1_sources.json 为下游真实依赖 | NOT_RUN（只有历史 #92 真实六帧 PASS） |
| W2 Mosaic | coverage→sample→upm_fit→upm_apply→reject→integrate→write（7） | P2NodeOp 真实调用 HiPS reader、采样/UPM/排异/归约/writer，若干 p2_*.json/.bin 物化；同 HiPS 阶的科学产品才可成为正确性能基线 | NOT_RUN |
| W3 Export | properties→wcs→resample2→writer→verify（5） | P3NodeOp 使用 HiPS properties、WCS、分块 resample、FITS writer、验证；p3_props.json、p3_wcs.json、p3_resampled.json/bin、output_phase3.fits、p3_verify.json 的跨节点文件通道 | NOT_RUN |

校准功能域已有 acsd_p1_calibration 动态库 target、v1 module_entry.cpp，但当前 CLI 上述 cal/cos factory 入口不能冒称已通过 ABI v2 动态符号运行；另外 CPU SIMD backend 有 profile 驱动动态 provider 加载，与科学 Operation DLL 是不同路径。旧 phase1/2/3_session 另有接口入口，其存在并不等于每个 CLI IR 节点都调用整段 session。

## 三、实际资源机制及限制（当前源码）

- **调度**：RuntimeImpl 的 NodeSpec 来自 module.plan（min/max workers、estimated_memory_bytes）和 estimate_plan；Scheduler 实际持 ThreadBudget、heavy_gate / heavy_mu，依赖 ready queue；同批在途租约分配与 cpu_heavy 互斥会影响有效并行。配置核数/计划 worker 与真实租约观察值必须区分，commands.cpp 的 ResourceRecorder 在无 active grant 观测时会回退到 planned 数值，因此不能把每一列 workers 都当真实忙碌 CPU 核数。
- **现有内存回压**：commands.cpp 的 CLI budget/profile 与系统内存探针经 run_pipeline 传给 RuntimeResourceBudget，RuntimeImpl 构造 MemoryPressureGovernor 并调用 scheduler_->set_memory_governor；Scheduler 用估计字节做排队/准入。估算不可得会登记 plan_unestimable_reason；估算是 NodeSpec 层，不是每次 malloc 的原子 Credit。choose_victim 或 evicted 记录也不是 CPU join→AIO drain→无借用→回收→attempt重试的事实证明。没有证据保证绝不 OOM。
- **Block**：现行 core/block_frame.h 有 create / consume 与声明 consumer 的复用模型，block_flow.cpp 有规格检查和有效期规则。但 P1/P2/P3 生产将大量实际科学工作面写盘再读，因此不能把 BlockFrame 视作当前 CLI 所有大数组真正 typed ReadLease/FrameBlockStore 的共享内存流水。需从实际 allocation/free 与 read/write 的临界窗口验证旧/new/scratch 重叠峰值。
- **CPU/AIO**：core/executor.h 提供 CPU heavy / IO executor；真实 P1/P2/P3 adapter 还执行同步 AIO read/write 和 OpenMP/内层工作单位。底层已有 AIO/HiPS/原子 rename 实现，但没有可直接证明所有计算 worker 均能在磁盘慢写时立即释放的 ticket/backpressure 全链实测。工作区写盘是实际计算依赖，不能当成仅诊断 I/O。单个文件原子 rename 不证明多文件产品原子提交。
- **诊断/基准**：生产 CLI 有 ProcessMonitor、ResourceRecorder、AllocationRecorder，生成 resource_timeseries.csv、resource_summary.json、alloc_samples.csv、alloc_report.json，Runtime 有 node trace/graphs。benchmark/backend_host/profile_gen_v2.cpp 是 CPU ISA/代表 kernel 的真实 benchmark（warmup、workers、median/MAD），不是 cal/IPV/PSF/UPM/Drizzle/AIO 每 operation 端到端物理代价画像。C11 ABI v2、typed store、MemoryCredit、TaskGroup 原子回滚均属 **TARGET / NOT_IMPLEMENTED**。
- **已知测试债**：#19 曾报告既有 26 failed / 56 passed；本批未运行，没有逐用例差异归类，状态必须为 HISTORICAL_REPORTED / UNCLASSIFIED，既不是本批新红也不是当前单测绿。#91 的局部 A/B 保留自己版本的证据。

## 四、三 Workload 冻结合同

运行命令见 [RUNBOOK.md](RUNBOOK.md)，测量工具见 [run_baseline.py](run_baseline.py)，当前状态见 [STATUS.json](STATUS.json)。

**W1**：直接消费 #92 已成功的 50-cfg6.json；唯一配置变动为运行机绝对根路径、可指定 Gaia 根路径及输出工作目录，六帧、Master、Baader R / Ideal QE curve、header_pointing、dark K=.5、Drizzle pixfrac .8、光度 fitted+spatial application 的科学键保持。执行前自动逐文件重算九输入 SHA，核对 Gaia 20 个文件名+大小。成功判据为真实 CLI exit0、run manifest complete、六帧 photometry fitted/applied 的 p1_phot 以及 frame_science、WCS、signal、HiPS/精度身份可读；当前不可采集，**NOT_RUN**。

**W2**：从 W1 实际生成的多帧 P1 HiPS 显式列出每帧目录；运行脚本不猜测产物结构，用 --w2-hips-json 传入确定的 JSON 字符串数组。强制同 hips_order、BUNIT 与可见响应/滤镜字段一致，存在真实 signal tile；科学人员仍须确认所有响应曲线/provenance 和 frame_science 能力没有 #9 类不一致。绝不采用 #19 跨阶拒绝负例、#80 异响应数值病态或历史 FAIL 产品作为普通 W2 基线。要求完整 writer 及 complete manifest。当前 **NOT_RUN**。

**W3**：用 --w3-hips 指向 W2 已合法 complete 的 P2 signal HiPS，并显式提供 --w3-complete-manifest；检查真实 properties/order/BUNIT/信号 tile，按 M42 天区配置中心、尺寸、尺度，再运行 P3。输出应有完整 FITS PRIMARY WCS / BUNIT / DATA 及 verify，并记录科学字段和哈希。#91 的正常/失败差异及 #87 的输入 tile 完整性风险需归档；#87 未闭合前必须主动排除已知损坏 tile。当前 **NOT_RUN**。

## 五、测量身份与探针缺口

每次有效运行输出：独立 run-NN/config.json、input_identity.json、stdout.jsonl、stderr.log、process_samples.jsonl、measurements.json、product 文件树及 session.json；脚本默认同一 workload 重跑三次、保留所有原始读数。冻结软件源码 SHA、实际 binary SHA，科学原始文件 SHA 与输出全树文件大小和 SHA 列表摘要。Linux /proc 针对 ACSD PID 提供墙钟、进程 user/sys CPU、VmRSS/VmHWM、线程数、read_bytes/write_bytes（受 cache 影响，属于设备 I/O 而非逻辑读写字节）；**不将未仪器化变量填零**。

| 指标 | 当前状态 | 需要的测点 |
|---|---|---|
| 当前版本 wall、CPU seconds、peak RSS、process I/O、产品大小/哈希 | NOT_RUN；脚本已给测法 | 真实 binary + 原始 FITS/Gaia；同时比较 CLI 自身 MON/alloc 原始 CSV/JSON |
| 逐 operation busy/idle、granted worker、节点等待 | UNINSTRUMENTED 或部分 CLI trace 可观测，未实跑 | RuntimeImpl NodeTrace；Scheduler task 开始/结束与实际 ThreadBudget lease，不把 planned 当 actual |
| AIO ticket/queue depth、背压等待时间、慢盘 worker occupancy | UNINSTRUMENTED | aio 具体 read/write entry/complete+调用线程归属、外部 pidstat/iostat/PSI |
| live allocations、每个 Frame 旧块/新块/scratch 过渡峰值 | UNINSTRUMENTED | 分配/释放与 consumer/借用最后释放点，和 alloc_samples/RSS 同时间轴 |
| estimate_plan vs 真正峰值、未知估算 | NOT_RUN / partial static | NodeSpec.estimated_memory_bytes、plan_source、unestimable 原因、HWM/活动帧数 |
| 科学完整性与独立 Oracle | NOT_RUN | 当前输出 frame_science、p1_phot、P2 COMPLETE、P3 FITS/verify；此前 #92 只为历史参考 |

## 六、ARS-02 精确迁移起点与停止决定

[冻结的 ABI v2 首切片规范](../../docs/engineering/api/abi/PROCESS_ABI_V2.md)：同一个既有 acsd_p1_calibration 功能域 DLL 的 calibrate_frame→ac_calibrate_frame、correct_frame→ac_correct_frame。初步只替换内建 cal/cos 核段；保留 Phase1 校准 master 预处理、Cosmetic 内的 ac_correct_columns_ex2 坏列补偿、上下游 WCS/photometry/noise/drizzle 及科学单位身份。后续依赖仍为 #24 DLL→#25 typed IR→#26 FrameBlock→#27 Credit→#28 rollback→#30 AIO→#31 revision→#32 Executor→#33 Barrier→#34 Bench→#35 ARS→#36–38 三阶段迁移→#39–41。不能为优化内存无意改变 #9、#10、#19、#80、#87 科学与数据协议。

**共享文件冲突：** module_adapters.cpp、根 CMakeLists.txt、core ABI 头、commands.cpp、wcs_transform.cpp 及科学 writer/reader，全部保持不变；本批只写 实验/ars-01-s0/ 外围证据文件，不修改 docs/** 正本。

**本批最终裁决**：STATIC_CONFIRMED 调用图 + HISTORICAL_VERIFIED 的 W1 成功输入与证据定位 + 脚本已落盘；但没有当前 HEAD 对应的 W1/W2/W3 完整真实测量，所以 ARS-01 **INCOMPLETE / NOT_RUN**，Issue #23 保持 Open。本任务在此停止，不开始 #24。
