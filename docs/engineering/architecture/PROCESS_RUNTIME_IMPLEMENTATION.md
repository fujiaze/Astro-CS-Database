# ACSD Process Runtime / ARS 分阶段实施路线

**用途**：供规划 Agent 分解、排期、确认依赖和分配执行节点；本文件不是已经完成的里程碑或研发审批制度。技术正本为 [PROCESS_RUNTIME_ARS.md](PROCESS_RUNTIME_ARS.md)，函数级接口与验收反例见 [ARS 可编码详细设计](PROCESS_RUNTIME_DETAILED_DESIGN.md)，科学定义仍以 [ACSD_DESIGN.md](../../ACSD_DESIGN.md) 及 `docs/science/` 为准。与功能域 DLL、多 operation、JS 编排、Frame–Block 生命周期及整帧重算共同实施，不另造第二个调度体系。2026-10-16 依据已核查的 main 基线编制；Agent 开工前读取实时 main SHA。

## A. 共同执行与协作要求

- 所有改动只在 `main`，按主题小提交并及时 push；无新分支、无 PR、无 force。多 Agent 并行前明确源码/文档文件所有权，避免对 `module_adapters.cpp`、`runtime_client.cpp`、公共 ABI 头并发写冲突。
- 先读最高设计、系统架构、ARS 正本、涉及的 science 正本、Runtime/Block/ABI/AIO 合同，再核对真实函数与生产调用。先区分**文档声明/注册表声明、可链接入口、正式 CLI 真实路径、单测、E2E 和独立科学数值验证**。
- Phase 不是阻止研发的人工门禁；前置关系表示实现的真实数据/ABI/所有权依赖。调度与科学代码分离，改变科学模型另开对应 science issue，不把资源优化当作修改 WCS、PSF、SNR、Mosaic 权重、单位或序列资格的许可。
- 优先复用 `lib/infrastructure/scheduler`、`lib/infrastructure/aio`、`lib/infrastructure/benchmark`、`module_ports.registry.json` 和现有系统探针，逐步取代重复实现；不要新建一堆平行“门禁”、“状态表”或第三套 manifest。
- 每个 Issue 交付：源码/API 或正式设计变更、真实调用证据、单元与针对性 E2E、实测内存/性能原始摘要、失败/未运行说明、main SHA、与关联 Issue 的状态。证据路径放所属实验单元，不上传用户真实 FITS 数据。没有运行就写 NOT_RUN。
- 资源安全优先：不能用 fail-open 把未估算内存当 0；不能通过删除原始输入 occurrence、静默丢弃科学帧或改变归约方法来取得吞吐；不能对绝对 OOM 免疫作无条件保证。

## B. 分阶段工作包（Issue 稳定标识）

| 阶段 | ID | 工作包 | 主要交付与职责边界 | 依赖 |
|---|---|---|---|---|
| S0 事实基线 | ARS-01 | 现有运行路径和资源基线 | 静态/CLI 动态入口、Block/AIO/内存/CPU 路径映射；冻结受控科学和性能输入、复跑脚本 | 无 |
| S1 模块接口 | ARS-02 | 功能域 DLL + multi-operation Host ABI | Operation descriptor/plan/execute 生命周期，ABI v2 兼容、Windows/Linux loader，实际动态调用 | ARS-01 |
| S1 工作流接口 | ARS-03 | Typed Process IR / Operation Registry | 唯一 operation/port 身份、schema、消费者边、科学参数与硬件计划分离；兼容旧 IR | ARS-02 |
| S2 数据面 | ARS-04 | FrameBlockStore + typed payload/lease | typed Block/model、run/frame/attempt/revision、不可变发布、零拷贝 move、真实消费者释放 | ARS-03 |
| S2 内存面 | ARS-05 | Memory Credits + 预留/准入/再评估 | 全局 live/reserved/reusable/I/O 账本、动态安全预算、过渡峰值、非估算 fail-safe | ARS-04 |
| S2 恢复面 | ARS-06 | Frame TaskGroup + 整帧 rollback/retry | quiescence、CPU/AIO 借用收尾、attempt 隔离、实际 reclaim 再重算、公平防饥饿 | ARS-04,05 |
| S3 数据流 | ARS-07 | Astro AIO BufferPool + 异步读写背压 | 有界 read/write buffer、host async API、completion、阻塞后端适配、计费 | ARS-04,05 |
| S3 产品提交 | ARS-08 | Frame/Mosaic/Export staging 原子发布 | 批次版本 manifest、signal+模型完整性、取消失败与 crash、一致读者规则 | ARS-06,07 |
| S4 Compute | ARS-09 | 单一 Compute Executor/资源租约 | 任务组、CPU/OMP 预算、work stealing、取消安全点、消除 redundant heavy_gate/线程池 | ARS-05,06 |
| S4 Planner | ARS-10 | 逻辑 DAG → 物理 tasks | frame wavefront、subunit partition、依赖同步、模型只读版本、可证的 fusion/reduction | ARS-03,04,09 |
| S5 Hardware | ARS-11 | 机器与 operation benchmark | CPU/ISA、内存带宽、I/O、线程扩展性、真实 kernel 画像、有效域和 fallback | ARS-02,09 |
| S5 Policy | ARS-12 | ARS 实时调度决策与遥测 | 双层资源准入+优先级、关键路径/效益/公平、动态 in-flight、I/O 压力调节、回滚策略 | ARS-05,06,07,09,10,11 |
| S6 P1 | ARS-13 | Normalize 正式生产路径迁移 | 八节点科学 DAG → Process/FrameBlock/ARS；逐帧波前、Gaia/AIO、全部真实 provenance | ARS-08,10,12 |
| S6 P2 | ARS-14 | Mosaic 生产路径迁移 | 全局 UPM/coverage 和局部 tile；共享状态与整帧重算边界；科学权重/排异不变 | ARS-08,10,12 |
| S6 P3 | ARS-15 | Export 生产路径迁移 | WCS/HiPS→FITS typed 局部工作、全局导出事务、受限写入队列 | ARS-08,10,12 |
| S7 Extensibility | ARS-16 | JS 风格工作流前端 | 静态主体 typed DAG、受控动态子图、三命令默认 workflow、可复现脚本版本 | ARS-03,10,13 |
| S7 交付 | ARS-17 | Windows/Linux 安装、插件能力和发布连接 | 两平台 DLL 生命周期、ISA fallback、插件 hash/registry/install、CLI/GUI 兼容 | ARS-02,13,14,15,16 |
| S7 实证 | ARS-18 | 真实 E2E、故障注入、性能优化实证 | 慢盘/内存竞争/取消重算/崩溃/长运行，科学对照与性能归因 | ARS-13,14,15,17 |

**注**：ARS-16 可以在 S1–S4 并行开发语法/IR 原型，但默认工作流替换需要生产后端真实完成；ARS-11 可以与部分 S3 任务并行采集 benchmark。ARS-13/14/15 属于同一接口冻结后的并行工作包，但共享 `module_adapters.cpp` 等文件要先切分所有权或顺序合入 main。ARS-18 的早期测试/基线随 S0–S6 持续建立，最终汇总与独立复核在后期完成。

## C. 每阶段关键实施设计

### S0：事实基线和科学等价基准（ARS-01）
按 `CLI -> IR -> ModuleRegistry -> Scheduler -> P1/P2/P3 adapter -> algorithm/AIO` 核验真实生产调用；区分实际动态加载器与 CLI 静态科学模块。建立对照数据清单（用户现有真实数据只建索引，不上传原始）；固定同一科学 config、输入 occurrence、原始 signal/sidecar/模型 identity。记录峰值 RSS、内存分配源、线程授予、CPU busy、每 op 延迟、AIO read/write/queue p95、总墙钟；缺指标如实列出。将旧 Frame/AIO/benchmark 实现与可复用接口映射，不改算法。

### S1：真正可复用的功能域 Process（ARS-02/03）

**ARS-02 首次编码的唯一头级口径**：[冻结的 v2 ABI + Calibration 双 Operation](../api/abi/PROCESS_ABI_V2.md)。先复用 `acsd_p1_calibration` 同 DLL 的 `calibrate_frame/correct_frame`，实例串行/帧间并行，严格保留旧 cal/cos 科学与坏列处理；所有 v2 Host callbacks 和所有权以该头为准。实施前先完成 ARS-01 #23 科学与性能基线，不能将 ABI 文档本身当成可运行证明。

稳定 `(module_id, operation_id, schema_version)`；`module_ports.registry.json` 不再只映射静态适配器，应与真实 DLL descriptor 和 IR 的同一能力面核对。C ABI 以 opaque host-owned block/model handle、POD/span 和版本协商跨边界；模块服务函数指针对象 pin 至所有 instance、views、jobs 完成。可测试 Linux/Windows secure loading、符号检查、ABI mismatch、卸载中的任务、多个 op 实际执行。先在一个小的真实生产算法做动态端到端证明，再推广，不以 DLL 加载成功冒充 CLI 真正执行。

### S2：Block 账本 / Frame 回滚（ARS-04/05/06）
`FrameBlockStore` 包含原始输入、过渡 output、模型引用、消费者集合、attempt/revision；producer 先写私有结果，publish 后只读；旧 Block 最后消费者结束才释放。所有高字节分配有原子 credit；估算含 `M_old + M_new + scratch + outstanding AIO` 的过渡峰值，避免重复计费 shared block；块创建失败可返回资源不足，内存不够不能超限“放行以推进”。保留用于完成至少一个已准入 Frame 下一次变换的推进额度。
帧取消不是 `evicted=true` 即完成；按 stop dispatch -> cancel -> join CPU -> drain/abort AIO -> release views -> reclaim actual -> retry。共享 Mosaic global model 和已合入贡献不参与 Frame rollback；禁止对不支持回滚的全局 op 擅自撤销贡献。失败重算必须维护原始输入全集，不得通过删除帧绕过性能问题；多次牺牲增加保护和缩并发。

### S3：异步 AIO 与提交事务（ARS-07/08）
用 MemoryCredit 管 BufferPool、编码 scratch、I/O 队列、inflight request；即使磁盘慢，compute worker 也不阻塞于普通写完成等待；需要 blocking file syscall 时由独立 I/O 工作执行。CPU 密集型 FITS 压缩/校验另按 CPU 预算计费。慢盘时 buffer 池先限制新增生产任务，公平运行独立计算任务；不能承诺磁盘永久慢而 CPU 永远满载。事务由 Host AIO 将同一 Frame 的 signal、科学 sidecar、model 引用、manifest 作为完整 revision 发布；中途取消/崩溃不能出现可消费半成品。Mosaic/Export 分别拥有适合其科学结构的产品 transaction。

### S4–S5：智能规划与执行（ARS-09/10/11/12）
operation 的 `plan(input_metadata)` 提供拆分轴、同步点、内存随 worker/work-units 增量、计算/I/O 字节、预测置信度。物理规划器以科学 DAG 为约束，将 Normalize 展开 frame wavefront、Mosaic 拆全局 barrier 与局部 tile；不可影响确定性归约顺序。CPU executor 统一预算、task steal 与取消；硬件画像测量 compute、bandwidth、SIMD、cache、I/O 及竞争退化。
调度 policy 先以依赖/credit/I/O slots 判定可执行，再在合法候选中用 critical path、预计剩余工作、增量内存、缓存亲和、IO backpressure、公平 aging 排序；观测更新估计但不修改科学参数。保存 plan vs granted vs measured 证据；不单凭 100% CPU 达成与否宣称成功。

### S6：三个真实科学工作流（ARS-13/14/15）
P1 迁移时保持 WCS 不依赖先验 header，明确 solver_detections/science_sources、applied photometry、噪声和球面 precision 一致性；不因任务交错让 frame_science 或应用标度依赖文件到达时序。P2 需要先固定 P1 canonical precision 的真实消费约定，详见现有 [#13](https://github.com/fujiaze/Astro-CS-Database/issues/13)，并尊重 [#8](https://github.com/fujiaze/Astro-CS-Database/issues/8)、[#9](https://github.com/fujiaze/Astro-CS-Database/issues/9)、[#10](https://github.com/fujiaze/Astro-CS-Database/issues/10) 的科学问题，不在调度重构中顺手改错权重/单位/HEALPix 量纲。P3 必须保证真实 WCS 几何、支持域及 FITS 头，与磁盘流式写入的一致性。

### S7：脚本、交付和完整实证（ARS-16/17/18）
脚本解析/执行环境仅构造完整 typed DAG 或受约束的动态子图；热路径原生 C++，每个 run 保存脚本/编译 DAG/算法实现 hash。Windows/Linux 的 loader、安装依赖、Qt GUI 的 JSON/CLI 兼容分别实测。真实帧与 HST 真值合成、不同分辨率、慢写设备、内存突然缩减、RSS probe 失效、Frame 连续重算、AIO 失败、进程 crash、科学全链结果差异和性能收益需有可重现记录。性能结果必须报告实际环境范围和偏差，不以虚构预测值充当实测。

## D. Issue 最小完成记录及相互依赖的处理

每个子 Issue 的完成报告需明确：
1. 实际 main SHA / 科学基线 / 变动文件与真实调用链（若仅原型，写明仍未接入生产）；
2. 核心设计和 ABI/data/ownership 兼容性差异，不得只给一页总结省略函数/所有权；
3. 构建及单元/异常/集成、生产 E2E 的具体输入身份和原始报告路径；未运行写 NOT_RUN；
4. 数值等价域、性能耗时分母、估算与实测、OS 压力观测、取消后实际释放的证据；
5. 新发现问题如何与已有 #5–#14 等科学修复独立跟踪，不因该实施 Issue 关闭而视为科学问题已经解决。

## E. 规划 Agent 的建议安排

先核对已经在跑的 Codex 文档整改与代码 Agent 的文件所有权和 main 提交。优先启用 S0 与 S1 接口设计，再让 FrameBlockStore/MemoryCredit 和 AIO/Executor 分工推进；涉及公共 ABI、`module_adapters.cpp` 或 `runtime_client.cpp` 的修改由一个节点在同一时间窗口负责合入，其他节点可以提供独立实现文件与测试。先证明一个真实 DLL Process、一个 Frame Block 过渡、一次整帧回收以及一次不阻塞 compute 的 AIO 提交组成的纵向切片，再扩大到三阶段。排期以依赖和实际构建成本决定，不依据本文件预测固定天数。

**任务清单索引以 GitHub 总 Issue 的实际编号为准；表内 ARS-XX 是稳定设计 ID。** 规划 Agent 可以在任务级提出更细的科学或性能缺陷 Issue，但应优先复用本路线中的功能交付，避免同一目标出现多份不一致标准。
