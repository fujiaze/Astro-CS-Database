# P0 架构、性能和科学一致性任务

这些任务来自最新源码包的独立抽查，不允许被“CI 已接入”替代。

## V8-RT-001｜现场生成真实运行图

- 从模块注册、scheduler、CLI dispatch 和实际执行符号提取图，不从 README 手写。
- 每节点记录 module ID、operation ID、entry symbol、input/output artifact、sync/async、parallel axis、provider。
- 检查多个节点是否错误绑定同一个一站式 session；声明的 artifact 是否真由代码产生和消费。
- 输出 machine JSON、Mermaid/Graphviz 图和差异报告。

## V8-RT-002｜Phase2 一节点一操作

当前嫌疑链 `coverage → sample → upm_fit → upm_apply → reject → integrate → write` 不得全部调用同一个 `P2Api/session_run`。

通过条件：

- 每个 IR node 有唯一 operation 与执行入口；
- 输入输出 artifact 在运行中真实存在并带 schema/hash；
- 调度器不会让每个节点重复完整 Phase2；
- 单节点重跑只执行该节点及明确依赖；
- 单元测试以调用计数/trace 证明每操作恰执行一次；
- Phase1/2/3 仍可独立调用。

## V8-P2-001｜重计算循环台账

逐项列出 coverage、sampler、UPM、rejection、integration 的 tile/chunk/frame/pixel 循环、复杂度、并行轴、共享状态、归约顺序、内存预算和入口。自动源码检查禁止 heavy loop 被标 serial/unknown，除非 SCI/算法证明不可并行并给出并行替代方案。

## V8-P2-002｜消除串行重计算

- 使用统一 runtime/thread budget，不为每模块自建不受控线程池。
- 选择结果独立的 tile/chunk/pixel 并行；归约固定语义，允许合同内浮点误差。
- IO/write/metadata 可串行，但 compute 子区间必须单独标记和监控。
- 用合成小/中/大规模验证 1-worker 与 N-worker 数值一致，并证明 N-worker 真正产生并行工作。
- 检查并修正 `P2_ENABLE_OPENMP OFF`、coverage 主循环、sampler 第二/三遍、UPM CG、rejection 像素栈、integration tile/chunk/frame/pixel 循环；可不用 OpenMP，但构建合同必须准确描述实际并行 runtime。
- `SessionModule::plan()` 不得再把 heavy operation 固定成 `work_units=1`；每个 operation 必须产生可观测工作分片。
- 验收证据必须包含真实 thread ID、逐线程 CPU 时间、工作单元计数和 `worker_balance.csv`；`parallel=true`、`parallel_ok=true` 和日志自报 workers 均不算证据。

## V8-CPU-001｜删除硬编码 workers

移除 `workers_=2` 等常量默认值。统一 budget 来自 affinity/cgroup/内存和用户上限。未 benchmark 只影响 ISA 选择，不能退化为单线程。

## V8-CPU-002｜CPU benchmark/profile

- provider：generic、AVX2、AVX-512（仅在编译和硬件支持时）。
- 每路径先与 generic/解析 Oracle 比较，再热身、多次测量中位数与波动。
- profile 绑定 CPU feature、OS、binary/provider hash、数据规模区间和生成版本。
- 不支持路径明确 NOT_APPLICABLE；不得非法执行探测。
- 没有/失配 profile：选择 generic ISA，但仍使用动态多线程。

## V8-CPU-003｜生产路由闭环

不能只 `ctx.set_provider("baseline")` 或打印字符串。trace 必须来自实际 provider/DLL/kernel 入口，记录 build ID、kernel ID、ISA、worker budget。测试替换 provider 并验证真实调用计数；profile 修改必须改变实际选择。

## V8-MON-001｜真实观测

替换外部注入的 active/runnable workers 作为证据。Linux 读取 `/proc/<pid>/task/<tid>` 或等价真实进程/线程 CPU；Windows 使用线程/进程系统 API。`active_workers` 从活跃线程样本计算；输出 `worker_balance.csv`。修正 PSS、system CPU、IO 字段语义和实际相邻采样时间；`rchar/wchar` 只能记字节，不能冒充 I/O ops。原始 samples 必须可重算 summary。

## V8-MON-002｜利用率门禁

- 对 compute 子区间独立计算 CPU 利用率、active threads、progress。
- heavy 任务即使短于 5 秒也不能因固定豁免而掩盖单线程；短测试可用增加重复量获得可测窗口。
- mixed 任务不能只因声明 compute/io 区间就 PASS。
- 伪造 worker/profile/trace 的负向测试必须失败。
- 内存持续增长、无进度和异常 IO 等待生成具体诊断。

## V8-SCI-001｜算法合成 Oracle

每个科学算法维护固定 generator、解析/高精参考、单位、不变量、边界、容差推导和随机 seed。覆盖 Calibration、PSF/WCS/Photometry、SNR/variance、Drizzle、UPM、rejection、integration、Phase3 projection。禁止用旧版本输出作唯一 Oracle。

## V8-DOC-001｜机器生成一致性

机器生成并检查：版本、当前 SHA、模块注册表、API/ABI、函数签名、SCI→ALG→DATA→API→SRC→TEST、Pipeline IR、运行图、线程/异步模型。固定输出 `version_consistency.json`、`module_registry.generated.json`、`api_inventory.generated.csv`、`traceability.generated.csv`、`pipeline_graph.generated.json`、`stale_reference_report.csv`，全部可由当前 SHA 重建。每模块 README 必须链接 SCI/ALG/API/TEST 并列真实入口、并发/异步和制品。清理活动文档中的 V5/V17/V19 等陈年版本标题；历史只留 archive。模块 summary 不得继续为 0；Python CI 依赖必须有 lock；Linux 脚本不得残留固定 Windows 路径。

## V8-RES-001｜恢复原任务

在以上 P0 通过后，根据对账台账完成 V7.1 中属于 alpha 发布范围的全部未完成任务。前台只从 `baseline` 归档提取当前 task 的 goal/acceptance，按 V8.1 `DISPATCH.json` 重写执行环境；不得把旧 worktree/Windows 编排传给子 Agent。`V8-RES-001` 只有在机器脚本确认发布范围任务全部 CLOSED/明确 NOT_APPLICABLE 后才能关闭，不能只因“已恢复派发”而 PASS。仍使用固定模块 owner、现有 main、原子 commit、GitHub CI；不重跑历史版本对比。
