# 执行与生命周期模型

> 上游：ACSD_DESIGN.md §8（软件架构）、§9（CPU 后端与资源）

本文件给出各执行路径的串/并行分层、锁与原子原语、取消与超时语义、内存驻留与错误传播。生产执行层与隔离面分列：**ACR / CUDA / GPU 面不接入生产**（最高设计 §1.4：ACR 源码保留为隔离实验、生产不可达），**HiPS 浏览器属工具分类（非发布）**（最高设计 §1.4：GUI 非目标，不进产品清单），**Phase1 编排并入 CLI 的 pipeline driver、无独立进程**（最高设计 §8.1）。发布与性能结论只引用生产层条目。

## 1 串/并行分层

| 路径 | 切分单位 | 最大并发 | 调度器 | 同步点 |
|---|---|---|---|---|
| Stage1 calibrate | per-pixel（单层 `n = w*h` 循环） | Runtime lease | OpenMP parallel for（static） | 无 barrier、无 tile 级同步点 |
| Stage1 drizzle | per-source-pixel candidate（确定性 stripe 分片） | Runtime lease | OpenMP + cache | 按 stripe 索引升序左折叠归约 |
| Stage2 sampler | per-control-cell | Runtime lease | std::thread pool | cell barrier |
| Stage2 UPM solve | full graph；per-obs 权重行 | 主迭代 1；权重 = lease | 主迭代串行；权重 per-call 池 | — |
| Stage2 block/reject/integrate | per-tile（动态认领） | Runtime lease | p2_parallel_for（std::thread） | 无 barrier，跨任务零浮点归约 |

调用线程与证据锚：

- **Stage1 calibrate**：调用线程 = calibrator thread。证据 = `lib/algorithms/calibration/src/calibrator.cpp`
  （`:94` 平场归一、`:126` 兼容式、`:134` 标准式；`calibrate_d` 的 `:170`/`:178` 同构）。
- **Stage1 drizzle**：调用线程 = drizzle worker。证据 = `drizzle_engine.cpp` `:1923`（主并行区）、
  `:1702`（`merge_tile_map_into`）、`:2117-2142`（stripe 序归约）。

- **Stage2 sampler**：调用线程 = stage2 worker pool；`cfg.cpu_workers = budget.max_workers`。
  证据 = `sampler.cpp` `:924-954`（`next_c.fetch_add(1)` 动态取 cell）。
- **Stage2 UPM solve**：调用线程 = stage2 main（IRLS 主迭代）+ per-call std::thread 池（`compute_raw` /
  per-obs 权重，登记在 `execution_options_contract.md`）。证据 = `upm.cpp` `:605`（`compute_raw`）、`:747`
  （per-obs 独立 w 计算）、`:620/:751/:794/:916/:2143`（per-call 池 ×5，`cworkers = cfg.cpu_workers`）；
  M/C 更新主体串行（`cg_solve_frame` `:676` 起）。
- **Stage2 block/reject/integrate**：调用线程 = block worker（std::thread 池）。证据 =
  `module_adapters.cpp` `:9045`（`p2_parallel_for` 定义）、`:10983`（reject 调用点）、`:11778`
  （integrate 调用点）；`rejection.cpp`/`integrate.cpp` 内 `#pragma omp` 计数 = 0。

确定性锚点见 `execution_options_contract.md` §6。ACR 异构分块面不接入生产（最高设计 §1.4），不在本表列行。

## 2 异步 I/O

| 项 | 模式 | 细节 |
|---|---|---|
| HiPS write | 原子提交（`aio_hips_writer`） | 同目录临时文件 → 内容写出 → CHECKSUM 校验 → `fsync` → 打洞（可选）→ 原子 `rename` → 父目录 `fsync`；同目标并发写为 last-writer-wins（单写者前提，见 `io/IO_003_ATOMIC_OUTPUT_PUBLISH.md`） |
| HiPS read | 并发只读（无进程级锁） | 读路径无进程级共享可变状态；`fitsfile*` 线程私有、不跨线程转移；并发安全由 cfitsio `_REENTRANT` 构建保证（详见本节末） |
| Fallback | sync fallback | 生产 fallback **只有一条**：无 cpu_profile → baseline 后端 + 动态 worker（保守合法，见 `CPU_BACKEND_ARCH.md` §6） |

**HiPS 读路径线程模型**：每次调用各自 open→read→close，句柄只活在调用栈帧内；
`FptrTable` 与错误栈由 cfitsio 自带 `Fitsio_Lock` 保护，`READONLY` 打开走
`fits_already_open` 直接返回、不复用句柄（`cfitsio/cfileio.c` `:1544`）。
机器判据 = `EXEC-AIO-READ-NO-GLOBAL-LOCK`（执行合同判据，载体见 门禁注册面（G08-10 重建））。

## 3 锁 / 原子与 I/O 串行

| 共享 | 原语 | 粒度 |
|---|---|---|
| aio_read 读路径 | **无进程级互斥量**（读路径不存在进程级临界区） | 每次调用独立句柄，句柄线程私有、不跨线程转移；剩余串行化点（诊断/写面）统一走计数式 `aio::CfitsioLockGuard`（等待进 `resource_timeseries.csv` 的 `lock_wait_ns`） |
| rejected_* | `atomic` | per-sample |
| Drizzle counters | `atomic`；浮点归约 = thread-local/per-stripe scratch 累加 + stripe 索引升序左折叠（无 OpenMP reduction 子句） | per-tile |
| Dense cache | `mutex` | per-write |
| Memory budget | `atomic` counters | per-alloc |

## 4 Future / Callback 与取消 / 超时

| 项 | 语义 |
|---|---|
| Orchestrator cancel | atomic flag `CANCELLED=9`（用户取消或超时；唯一源 `lib/infrastructure/cli/exit_codes.h`），流水线中断检查点 |
| Stage2 signal | handler 设置取消标志，当前 block 完成即退 |
| Timeout | stage 配置 timeout_ms，超时返 `CANCELLED=9`（唯一源不设独立超时码，取消与超时同码） |
| Exception 传播 | C ABI 边界捕获转返回码，无异常跨 DLL |

## 5 CPU 内存驻留与回退

| 项 | 语义 |
|---|---|
| CPU buffers | `BufferBinding` caller-owned，`free` via aio_hio_free |
| Fallback | **生产 fallback = 无 cpu_profile → baseline 后端 + 动态 worker**（见 `CPU_BACKEND_ARCH.md` §6） |

## 6 确定性与嵌套并行限制

| 约束 | 规则 |
|---|---|
| 浮点求和顺序 | 按输入索引固定顺序，reduction 文档化（见 `execution_options_contract.md` §6） |
| 输入顺序 | frame_id/cell/pixel 索引固定，不依赖线程调度 |
| 嵌套并行 | 外层已并行则内层串行 |

## 7 错误 / 异常传播

| 错误 | 传播 |
|---|---|
| C ABI 返回码 | 0=OK，非 0=失败，err 缓冲仅日志 |
| Invalid / UNDERDETERMINED | per-pixel status，不抛异常 |

## 8 执行合同面映射

本文件的执行合同面按执行路径标识，覆盖如下；生产错误面唯一源 = `lib/infrastructure/cli/exit_codes.h`（表见 `ERROR_HANDLING_STANDARD.md` §7）。

| 合同面 | 覆盖 |
|---|---|
| ARC-EXEC-001 | Stage1 calibrate：per-pixel OpenMP（`#pragma omp parallel for schedule(static)`，共享 P2_ENABLE_OPENMP 构建开关；线程数由 Runtime lease 注入） |
| ARC-EXEC-002 | Stage2 sampler 并发只读（无进程级锁；句柄单线程私有、不跨线程转移；Runtime lease 定 worker 数） |
| ARC-EXEC-003 | Stage2 UPM solve：IRLS 主迭代串行；compute_raw/per-obs 权重按 Runtime lease 并行（per-call std::thread 池 ×5，登记在 `execution_options_contract.md`） |
| ARC-EXEC-004 | Phase2 block/reject/integrate：p2_parallel_for（std::thread）tile 级并行、原子计数动态认领，无 barrier，跨任务零浮点归约；tile 内逐像素候选栈串行处理 |
| ARC-EXEC-006 | HiPS 写事务：临时文件 → 内容 → CHECKSUM 校验 → `fsync` → 打洞（可选）→ 原子 `rename` → 父目录 `fsync`；取消/失败不落正式产品 |
| ARC-EXEC-007 | Orchestrator cancel/timeout propagation |

子契约见 `execution_options_contract.md`、`io/IO_003_ATOMIC_OUTPUT_PUBLISH.md`、`ERROR_HANDLING_STANDARD.md`。

## 9 编排策略承接

调度器按下列策略编排，内存占用永不越界（最高设计 §8.3）；各策略的现行落点：

| 策略 | 现行落点（详见下） |
|---|---|
| **静态预算** | 标定常数 + 准入预算面 |
| **探针校正** | 探针面 + 探针驱动标定 |
| **异步并行** | 多帧并行 + I/O 与计算重叠 |
| **可中断排队** | 内存闸门限帧在飞数 |
| **可丢弃重跑** | 原子单元不落盘 + 重跑换目录 |

- **静态预算**：从输入数据（帧尺寸与类型、配置、模块声明）静态估算每模块的内存与 CPU 需求，
  作调度决策输入；标定常数落点 = `PERFORMANCE_MODEL.md` §1.2（`kP1FrameBytesPerPixel`、
  `kP1FrameMemSafetyFrac`、内存预算百分比），准入预算面 = `eng/packaging/config/runtime_resources.json`。
- **探针校正**：每节点墙钟 / 排队等待 / 块生命周期 / RSS / I/O / worker 均衡 / 缓存命中探针
  随事件流落盘（探针面见 `observability/RESOURCE_MONITORING_CONTRACT.md`）；用实测校正静态模型，
  探针驱动的标定旋钮 = `PERFORMANCE_MODEL.md` 与 `execution_options_contract.md` §3 的轴分配。

- **异步并行**：预算充裕时异步启动独立工作流——多帧并行（`p1_parallel_for` 原子认领分批，
  同时最多 `cap` 帧在飞）、I/O 预取与计算重叠（`ASYNC_IO_CONTRACT.md`）；
  异步只用于能隐藏延迟的 I/O、预取与压缩。
- **可中断排队**：预测后续工作流将产生内存膨胀或多线程峰值时，中断优先级低的工作流，
  让其在内存中等待、按序排队进入；现行形态 = 内存闸门 `p1_memory_cap` 限制帧在飞数
  （`execution_options_contract.md` §3「并行轴分配」），准入边界优先于并行宽度。
- **可丢弃重跑**：内存仍不足时丢弃进度最低的工作流并释放其占用，该工作流随后重新开始；
  现行形态 = 失败或取消的产物不落盘（原子单元不落盘），重跑 = 新运行目录 + 新 manifest
  （`ARCH-001.md` §5）。

**不变量**：数值结果与并发度无关——1/N worker 数值等价，判据是事前冻结的浮点容差；归约顺序冻结是达成手段，不是判据本身（最高设计 §8.3）。

## 10 在役生产 / CI 面

- `lib/infrastructure/cli/runtime_contract.h`：由 `lib/infrastructure/cli/commands.cpp` include 并**编入产品 `acsd`** ⇒ **在役生产**。
- `lib/infrastructure/cli/mode_gate.h`：同链 include ⇒ **在役生产**。
- `lib/infrastructure/scheduler/budget.py`：由运行闭包判据调用其 `selftest`、由 `eng/tools/quality/runtime_oracle.py` 锚定（运行闭包判据载体见 门禁注册面（G08-10 重建））⇒ **在役 CI 面**。
- 上述三者与隔离面（ACR / CUDA / GPU、Qt 浏览器）**分属不同类别**，各列一张表。

## 11 治理任务执行模型

本节规定**工程任务面**的执行流程（第 1–10 节规定的是运行时执行面，两者不互相覆盖）。

### 11.1 两条独立流程

治理工作分两条互不混用的流程：

| 流程 | 起点 | 产物 | 终点判据 |
|---|---|---|---|
| 制作 | 决策方给出方向（哪个阶段 / 模块 / 关注什么 / 不接受什么） | 差异审计表 + 工作包（任务书集合） | 工作包获批 |
| 执行 | 决策方指示执行某个已批准工作包 | 自证材料 → 前台独立验证 → 统一提交 → 汇总报告 | 每任务状态 PASS 且汇总报告在位 |

制作与执行不在同一次会话或同一条流水线内完成；制作任务停在「工作包写好并获批」，执行任务从「指示执行某个工作包」开始。

### 11.2 差异类型枚举

差异审计的每条差异归入且仅归入一类：

| 类型 | 含义 |
|---|---|
| 缺口 | 权威文档要求的机制在仓库中不存在 |
| 违规 | 实现与权威文档的条款冲突 |
| 过时 | 文档已声明退役、实现仍按退役形态存在 |
| 漂移 | 实现偏离了文档写明的细节 |
| 无主 | 有代码无合同（无任何权威条款覆盖） |

无法判定归属的差异登记为未决项，不强行归类。

### 11.3 工作包结构与任务书要素

```text
<工作包 ID>/
├── 00_README.md      启动文档：目的、方向、范围、审批、入口
├── TASK_LIST.md      任务列表：总览表 + 依赖图
├── tasks/<ID>-<序号>.md
└── GAP_AUDIT.md      差异审计表
```

- 工作包 ID 规则：`<阶段>-<批次>`；
- 每个任务书必含五要素：**目标**（本任务让什么符合哪条权威条款）、**权威依据**（条款号 + 公式或适用域）、**改动范围**（允许改的文件域，与其他任务互斥）、**步骤**、**验收门**（可机器复跑的命令与断言）、**禁止项**（不改科学公式与默认容差、不在主线外开分支）；
- 一个任务 = 一个可独立验证的提交；科学、架构、性能、文档改动不混在一个任务里；大模块按「合同 / schema → 算法实现 → 测试 → 集成」拆小步；
- 随包文档按 `DOCUMENT_GOVERNANCE.md` §10 处理：只作参照副本，不替换正本、不进提交面；
- 调度形态两类：任务之间有依赖的走串行单飞（前一任务 `PASS` 后才派下一任务）；无依赖且文件域互斥的并行派发；挂账与续派以台账追加为准，派单前列出本单的精确文件列表并与已派单逐一比对。

### 11.4 任务状态词表

`NOT_STARTED → IN_PROGRESS → PASS / FAIL / BLOCKED`。`PASS` 只由前台独立验证后写入。

任务状态词表与模块状态阶梯（`docs/ACSD_DESIGN.md` §12.5）是两个域，取值互不代用：模块交付状态不用任务状态词表述，任务状态也不用 `VERIFIED` / `IMPLEMENTED` 表述。

### 11.5 前台独立验证三层（缺一不可）

1. **机器门**：检查项、单测、合同检查器全部 rc=0；
2. **证据复跑**：前台独立复跑关键验收命令，不采信执行方的自述结论；
3. **文档-代码一致性**：改动与任务书声明的文件域一致，未越界。

判据边界：

- **「能编译」只证明构建通过，不等于验收通过**；验收通过的判据是断言或测试实跑 rc=0；
- 机器门红灯即任务 `FAIL`，本模型的豁免面积为零；失败上报带可复现命令与原始日志，工具或环境原因同样给复现；
- 台账、清单与状态登记一律按线索处理，不作结论；引用它们时须附本轮自取的机器证据。

### 11.6 统一提交

全部 `PASS` 任务由前台统一按序提交；执行方不参与 git 操作。提交守则化序列：台账追加 → 精确 `add` 目标路径 → 原子提交 → 严格模式复核（远端、本地与跟踪分支三 SHA 一致）。

### 11.7 收口即清理

- 全部任务 `PASS` 后写汇总报告：目标、差异闭合情况、每模块状态（取值集合 = `docs/ACSD_DESIGN.md` §12.5 词表）、遗留项；
- **真实数据终验先于工作包完成声明**：终验未过不写「完成」；
- **收口即清理**：汇总报告中具长期价值的结论沉淀进正式文档（最高设计 / 科学正本 / 工程正本 / 索引），随后把该工作包目录从仓库移除，并与清理动作同一提交；过程与证据由 git 历史承载，不在工作区留存；
- 仓库只保留最新生产代码、自解释文档集、合同与测试、证据目录，以及当前在执行的工作包；过期报告、归档目录与一次性审计工件在对应工作包收口时甄别处理，清理与功能改动分开提交，清理提交给出删除清单与依据；
- 不产 zip、胶囊、台账等重量级长期设施。
