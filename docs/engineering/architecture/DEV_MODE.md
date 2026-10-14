# ACSD 开发诊断构建与发布构建：唯一编译开关

> 工程架构正本。上游：docs/ACSD_DESIGN.md §1、§8–§10、§13；探针插入/命名/格式的规范见 docs/engineering/standards/DIAGNOSTIC_PROBES.md。2026-10-14 目标设计（**不是当前代码已全部实现的声明**）。
>
> 本文规定**一个公开的编译期主开关**、两种构建、开发诊断的边界和科学数据通路不变量。已有旧实现的事实见文末“现状与迁移”；不把当前热路径“写盘再读回”包装成设计目标。

## 1. 用户要求与核心不变量

ACSD 的三阶段科学产品不因构建方式改变：normalize 逐帧 HiPS 及必要跨阶段辅助数据，mosaic 最终**单 signal 科学图像通道的 HiPS**，export 普通二维 WCS FITS。用户自由选择不同相机与滤镜合并的权限也不变。

开发版本可以**额外**保存完整或局部科学中间量，且可以采集详细性能事件，用于 P1–P5 单点数值/坐标/统计比较和 CPU/I/O 调优。发布版本**没有这些开发诊断能力的实现及触发入口**，但保留正常科学处理所必需的日志、manifest、来源记录、合法性错误和程序内最低限度运行状态。这不是用开关对不同科学功能做许可或质量分级。

**核心科学中立性**：构建 ON/OFF 执行**同一份数值算法和数据依赖**。除了诊断自身耗时、资源和失败状态外，不允许开关控制科学分支、数据来源、缓存语义、线程调度决定、结果判据、异常排异或输出内容。

## 2. 唯一公开构建开关

CMake 公共入口：

```cmake
option(ACSD_DEV_MODE "Compile ACSD internal data snapshots and detailed performance probes" OFF)
```

目标仅有两档：

| 构建档 | 方式 | 二进制包含 | 行为 |
|---|---|---|---|
| 发布构建（缺省） | `-DACSD_DEV_MODE=OFF` | 科学算法、规范 HiPS/FITS、必要日志/manifest | **不编译**中间数据诊断 writer、详细性能探针实现及故障注入；即使环境变量设置也不会启用 |
| 内部开发构建 | `-DACSD_DEV_MODE=ON` | 发布科学算法 + 受控 snapshot/perf 诊断实现 | 通过开发运行选择器决定观测哪些节点/保存多少数据；不改变科学计算 |

`CMAKE_BUILD_TYPE=Debug/Release/RelWithDebInfo`（或 MSVC 多配置的 `--config`）是优化与符号配置，**不等于** ACSD_DEV_MODE。为公平测性能，`ACSD_DEV_MODE=ON` 可配 `Release` 优化和调试符号；发布版同优化选项测基线。

这一个开关是**唯一决定能力是否被编进二进制的配置**。开发版运行时的节点选择、ROI、采样间隔和磁盘配额属于**记录范围/开销策略**，不是第二个公开的功能编译开关；它们无效时不能改变科学逻辑。为大规模图像控制体积，默认允许性能汇总，**大数组快照缺省不自动全量落盘**，由内部开发配置显式选择。

CMake 实施时：
- 对所有包含探针调用的目标传递一个可信、同值的 `ACSD_DEV_MODE=1/0` 定义（或生成的唯一配置头），避免多个动态库编译取值不一致；
- `OFF` 时开发专用实现 TU 不加入目标，所有开发探针调用经 `#if ACSD_DEV_MODE`/宏编译消除；无开发 writer/probe 符号、JSON/文件打开操作、诊断计时、诊断计算及环境变量读取；
- `ON` 时诊断 sink 和计时 API 由公共基础设施实现，业务模块只调用抽象接口，**不**逐模块建文件、写 JSON、打开 FITS；
- 由同一主开关派生当前内部使用的 `ACSD_PROBES` 宏定义（若为了兼容旧头），但不能将 `-DACSD_PROBES=ON` 保留为可独立激活发布诊断的用户入口。

## 3. 清理既有多开关分叉

当前仓库根 CMake 真实存在：
- `ACSD_PROBES`：独立默认 OFF 的性能库构建选项；旧 `acsd/probe.h` 在 OFF 时宏为空，ON 时需 `ACSD_PROBE_LOG` 才写；
- `ACSD_DEV_MODE`、`ACSD_DEV_INTERMEDIATE_CACHE`、`ACSD_DEV_CHECKPOINT`、`ACSD_DEV_PROBES`：一个主项加三个子项；
- 仅部分 target 收到相应 compile definitions，不能据此声称三阶段探针已经一致；
- `ACSD_NODE_TRACE` / `ACSD_LEASE_TRACE` / `ACSD_UPM_PROFILE` 等旧环境探针依然可能不受开发构建约束。

**目标整改**：对用户和发布打包只保留 `ACSD_DEV_MODE` 一项作为开发仪表能力选择。合并旧 ACSD_PROBES 到该主开关，尽量**复用**现有 `lib/infrastructure/observability/probes/include/acsd/probe.h`、`src/probe.cpp`、AIO I/O 与 runtime trace，不新建第二套时钟/探针库。旧子项在迁移后不再是独立 public build option；迁移过程中可暂作内部由主开关导出的别名，但绝不能让它们在主开关 OFF 时启用诊断。

**特别区分检查点/续跑**：`ACSD_DEV_CHECKPOINT` 的旧语义是“跳过已完成帧/改变后续运行路径”。这**不是**纯被动仪表能力。若产品未来需要断点续跑，应作为单独的业务功能与数据正确性方案论证，不受诊断主开关偷偷控制。本次开发模式任务只允许“检查点内容的被动快照”，不允许借调试开关启用/禁用科学任务的跳算行为。

## 4. 什么必须一直存在，什么只属开发诊断

| 数据或行为 | 发布构建 | 开发构建 | 说明 |
|---|---|---|---|
| normalize/mosaic/export 正式 HiPS/FITS | 必须按产品合同生成 | 完全同样生成 | 包括阶段一**必须给阶段二**的稀疏 SNR、Gaia 光度/PSF 元数据，不是可关闭的 debug dump |
| 运行 manifest、必要来源/错误日志、普通运行事件 | 保留 | 保留 | 正式运行记录不属于开发详细探针 |
| 计算真正依赖的中间数据、必要原子 staging | 内存或必要暂存；不允许丢失依赖 | 同一科学依赖路径 | 临时 spool 如算法确需磁盘，应明确定义生命周期，不能借 ACSD_DEV_MODE 把它伪装成可有可无 |
| 节点中间平面/HiPS tile、控制点/权重/残差快照 | **不写**独立调试工件 | 仅选中的快照额外写，默认不全量 | 绝不由后续科学模块读取 |
| 详细 RAII 分段计时、采样热点、开发性能 JSONL | **不编译** | 按运行选择器启用 | 不能与正式日志异常处理耦合 |
| 注入故障/替换算法的实验旋钮 | **不包含** | 独立测试专用路径，不能混入被动探针 API | 有意改变科学结果的行为需另行明示为实验，不得叫性能探针 |

中间开发快照不是 HiPS 的第二个科学图像层。最终 mosaic 的产品树仍然只有 signal；把 `p2.integration.variance`、`p2.sky.delta` 保存为 **开发工作区数据**不改变正式产品定义。

## 5. 正式科学管线与被动观察如何分开

推荐拓扑：

```text
科学模块的有效输入块
        │
    真实数值运算  ─────────→ 具名数值输出块 → 下游科学消费者
        │                          │
        └── 计时/计数(只读)          └── SnapshotProbe(只读借用或受控拷贝)
                                         │
                                     开发专用 sink → 调试工作区
```

不可接受的拓扑：科学模块写一份中间文件 → 由是否开发模式决定后续读哪个文件、是否改用原始帧、是否跳节点。需要暂存的真实计算依赖必须无条件存在，并与快照副本完全分开。

内部科学对象生命周期由 `pipeline` 的块与所有权合同决定；探针如果异步写出，必须在数据块回收前完成**拷贝/引用转移**，不能在线程返回后借用悬空指针。快照不能写回、修改有效位、改变算法返回值；在快照功能关闭时禁止额外构造完整中间数组。

## 6. 工作区、性能影响与失败处置

- 一次运行的专用诊断根应由 runtime 统一生成，建议放 `<run_work_root>/_dev_diagnostics/<run_id>/`，不得置于正式 `signal/`、HiPS `Norder*/`、用户最终 FITS 或发布清单的**科学产品**集合内；也不向未授权全局临时目录或源码树输出。建议同级工作区，实际受统一 I/O 路径/沙箱规则约束。
- `snapshots/` 存选中的中间科学数组或摘要，`perf/` 存性能事件，`debug_manifest.json` 记录何时、为何、从何数据对象得到快照、选择器、单位和写出结果。调试 manifest 单独登记并明确 `non_scientific_product=true`，不纳入科学 HiPS/FITS 的下游输入扫描。
- 性能探针使用 steady/monotonic 计时，耗时字段单位明确，事件按 thread + stable node id 组织；线程内聚合、批量写出/低优先 I/O，不在每个像元循环抢全局日志锁。
- 开发快照允许按稳定标识选择帧、tile、ROI、次数和总字节预算；默认大数组全量 dump 不开启，永远不能因为快照队列拥堵阻塞科学计算。若无法无阻塞地取快照，允许丢弃该诊断项并登记 `SKIPPED_BUDGET` 等原因。
- **仅开发诊断写盘失败**：不改变正式计算结果、科学任务退出码和科学产物原子发布；对开发运行日志/摘要保留诊断失败计数与去敏错误。**正式科学文件和正式运行日志写盘失败**仍按原 I/O 错误合同处理。失败后不留被误识为完整的快照。
- 诊断必须能在正常成功、取消/中断和正式计算失败时尽量完成 bounded flush；失败记录如实，不因探针本身失败而遮蔽科学异常。
- 真实用户 FITS 不随 debug snapshot 提交到 Git；大型快照按运行清理政策回收，科学实验脚本、来源/哈希与小型结果保留；绝对路径/个人标识和凭据按正常日志隐私规则处理。

## 7. ON/OFF 结果与性能复核

在输入、配置、随机种子、CPU ISA、worker 数、数值归约顺序一致、且无被动 I/O 扰动导致调度分叉的可复现条件下：

- `OFF` vs `ON（未选快照）` vs `ON（选部分快照）`：输出 HiPS/FITS 有效区域、标度、WCS、源/背景数据一致；优先逐位相等，若因并行/浮点归约表现不同，应具体解释数值界并检查探针是否修改调度/数值路径。
- Release 二进制中应不存在开发专用 sink 的链接符号和从环境启用快照/详细计时的路径；设置旧 `ACSD_PROBE_LOG`、`ACSD_NODE_TRACE` 等环境变量不得在发布版触发其详细调试路径。
- 同一 Release 优化与机器/帧集比较 `OFF`、`ON+diagnostics=none`、`ON+perf` 和 `ON+selected snapshots` 的纯计算耗时与 I/O 量；**探针开销本身分开测**，不能拿启用快照的墙钟时间当发布模式算法性能。
- 文件完整性：每个选定快照的元信息和数值 hash 可追溯到一份确定的、处理阶段明确的临时数据对象；不得通过复制最终产物冒充中间节点快照。
- 正/反例验证关闭了快照依然处理出相同信号、快照磁盘故障不损坏正式产物、数据缓冲被回收后写盘不会 UAF、并发执行没有文件名碰撞/伪时间序。

测试为科学设计及工程测量的一部分，不设置自动审批式发布门禁。任何未实现项明确写为待修代码，不因本文存在而宣称已完成。

## 8. 当前源码状态与需要 Agent 一并修复的缺口

1. `CMakeLists.txt` 确有 ACSD_PROBES 与 ACSD_DEV_MODE/三个子项两条配置路径；现状不满足单开关目标。
2. `docs/engineering/architecture/DEV_MODE.md` 旧版曾明确记录：部分 p1_calibrated/photometry 热路径先写盘再读回，编译分支可能改变缺失时的回退行为。必须先清除计算依赖分叉，不能只用 `#ifdef` 隐藏 writer。
3. `lib/infrastructure/observability/probes/include/acsd/probe.h`、`src/probe.cpp` 已有稳态计时、线程局部批量事件的可复用实现；它不是全新未实现功能。
4. `lib/infrastructure/scheduler/src/module_adapters.cpp`、`lib/algorithms/coverage/tools/stage2.cpp`、`lib/algorithms/drizzle/healpix_drizzle/` 等是候选快照调用点，但要按真实数据流核对对象的生命周期与坐标单位。
5. 内部 `ACSD_NODE_TRACE/LEASE_TRACE/P1CAP_TRACE/UPM_PROFILE` 与 `ACSD_*_FAULT` 路径要统一归类；只读性能细探针进 ACSD_DEV_MODE，被动快照也进该模式；故障注入必须留在专用实验模式/测试文件，不给发布版留触发入口。
6. 计算依赖的 checkpoint/断点续跑属于另一个产品设计问题，本轮不靠开发开关开启/关闭跳算。

实施细节、探针清单、记录格式、格式化数值/坐标的约束，唯一权威见 standards/DIAGNOSTIC_PROBES.md。代码提交/复现证据由实验/engineering-evidence/acsd-science-docs-p5/SCIENCE_V2_AGENT_TASK_PACKAGE.md 承接。