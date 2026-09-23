# 插件文档：scheduler + pipeline（调度与资源）

> 上游：ASTROCS_DESIGN.md §8.1（顶层结构）

## 1. 职责与边界

- **职责**：typed DAG 的执行（注册、依赖、调度）、统一线程预算、**locality-aware 编排**、流式内存管理、资源监控、取消与 checkpoint。
- **不是**：不定义科学公式；不产生科学值；模块注册/加载与 ABI 校验由调度器承担（本模块负责执行与资源）。**模块名只有一份 = `scheduler` + `pipeline`（最高设计 §7.1）；`runtime` 不是模块名（禁第二名字）**；本页文件名 `19_runtime.md` 是登记在册的文档路径，不得作为职责名引用。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §8.1（顶层结构：scheduler + pipeline 职责名全仓唯一）、§9（CPU 后端与资源：内存极简化、编排连续性）
- `docs/design/*_DETAILED_DESIGN.md`（节点与先后关系）
- `docs/plugins/infrastructure/21_observability.md` §8（G-RES-01 资源门）

## 3. 输入/输出数据合同

- **输入**：run-plan（节点图、模块 ID、配置、输出路径）、cpu_profile、内存预算（可选）。
- **输出**：run-graph、run-trace（JSONL）、资源时间序列、artifact-manifest、run-summary；调度指标（worker 空转率、缓存命中率、数据搬运量、上下文切换次数、RSS 峰值）。
- 参考：`eng/contracts/schemas/run_*.schema.json`。

## 4. 算法与公式要点

### 4.1 DAG 与线程预算

- typed DAG：节点 = 模块/entrypoint/operation；科学依赖不可改变（最高设计 §3/4/5）；
- 一个进程只有一个资源调度器与线程预算源；模块不得硬编码 workers、不得私建长期线程池；
- 分块/并行只改变执行，不改变归约次序或科学结果；
- **并行轴分配（PERF-501）**：Phase1 节点的帧级宽度与帧内 OpenMP 度由同一 lease 预算切分，
  `in_flight = min(n, frame_workers)`、`inner_omp = max(1, thread_budget / in_flight)`，
  两轴之积 ≤ 预算。帧级被 `p1_memory_cap`（内存闸门）压低时必须把剩余预算转给帧内轴，
  否则出现「16 核预算只用 2 核」的利用率塌陷（实测 cpu 恒 202%）。
  语义与不变式见 `docs/architecture/THREADING_MODEL.md` §并行轴分配；
  标定值与实测见 `docs/architecture/PERFORMANCE_MODEL.md` §PERF-501；
  观测面 `ASTROCS_{LEASE,NODE,P1CAP}_TRACE=1` + `eng/tools/monitoring/node_waterfall.py`。

### 4.2 编排连续性与数据局部性

```mermaid
flowchart LR
    subgraph BAD["低效编排（避免）"]
        A1["块A 阶段1"] --> B1["块B 阶段1"] --> A2["块A 阶段2<br/>重新加载/缓存失效"] --> B2["块B 阶段2"]
    end
    subgraph GOOD["locality-aware 编排"]
        G1["块A：阶段1→2→3 一次走完"] --> G2["块B：阶段1→2→3 一次走完"]
        G3["块C：阶段1→2→3"] -.块间流水.-> G2
    end
```

- worker 领取一个数据块后，把该块上**已就绪且可本地执行的连续节点一次做完**再交还（depth-first over the block's ready chain），减少中间落盘/重载与上下文切换；
- 块间用流水线并行填满 worker（一块在算时另一块在 I/O），块内不做阶段间来回切换；
- 调度决策携带数据位置信息（块在哪个 worker 的缓存/内存中），优先把后继节点派给持有该块的 worker（work stealing 仅在该 worker 队列耗尽、会造成空转时发生）；
- 同一外部数据（Gaia 查询、PSF 模型、标定母版）的消费节点在编排上邻近，扩大缓存命中；
- 调度器输出编排指标：worker 空转率、上下文切换/抢占次数、跨 worker 数据搬运量、缓存命中率，作为 L2 性能验收证据。

### 4.3 流式内存管理

- 工作集 = 当前在算的块 + 其显式依赖；块完成即释放中间数组（引用计数/作用域绑定），不累积整轮数据；
- 可现场计算的量（逐像素逆方差权重、天光面值、投影坐标）按需计算，不预分配稠密数组（与最高设计 §8.2、§9 一致）；
- 缓存分层：进程内只读共享缓存（Gaia/星表、PSF、母版，带字节预算 + LRU）+ 磁盘缓存；缓存命中不改变科学结果；
- 内存预算 `memory_limit` 给出时，调度器据此选块大小与并发块数（**内存不是门禁**：最高设计 §4.5「资源门只管磁盘——内存/CPU/线程不设门」）；
- 大对象单一所有者、显式交接，避免多副本驻留。

### 4.4 取消、checkpoint 与监控

- 取消：协作取消 → checkpoint → 干净退出；
- 资源监控：进程/线程 CPU、RSS/PSS、内存增长、读写字节、I/O wait、work units、队列深度、worker 均衡、进度、墙钟；
- 重计算负载受 G-RES-01 **磁盘门**约束（内存/CPU/线程不设门；判据与 exit 10 见 21_observability §8 与最高设计 §4.5/§9）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `workers` | 由 profile | —— | 线程预算（来自 cpu_profile，不得硬编码） |
| `block` | 由 profile | —— | 分块大小（结合内存预算自动收窄） |
| `checkpoint` | true | —— | 是否支持 checkpoint |
| `memory_limit` | —— | MB | 内存预算（可选） |
| `cache_budget_mb` | —— | MB | 进程内共享缓存字节预算（LRU） |
| `schedule_policy` | `locality_first` | —— | 调度策略（locality_first/balanced） |

## 6. 接口/ABI

- entrypoint：run-plan → 执行 → run 产物；
- 模块通过注册表加载（版本化 C ABI 校验），不隐藏整阶段 Session；
- 缓存以只读共享句柄向模块提供（如星表客户端），模块不自行持有重复副本。

## 7. 错误与边界

- ABI/签名/CPU 特征不匹配 → exit 5（BACKEND）；
- 执行失败 → exit 6（COMPUTE）；
- **磁盘写满 / 写盘失败 → exit 10（RESOURCE）**；内存/CPU/线程不设门（最高设计 §4.5，退出码见 §7.2）；
- 取消/超时 → exit 9（CANCELLED）；
- 内存预算内无法安排最小工作集 → 显式失败并报告所需工作集，不静默退化。
- **退出码唯一源 = `lib/infrastructure/cli/exit_codes.h`**（本页不复制定义第二套数值表）；域→码映射唯一源 = `docs/contracts/LOG_AND_ERROR_CONTRACT.md` §5。
- **模块错误必须上行到 CLI**（最高设计 §7.3）：节点/模块的失败以稳定错误码返回并终止本阶段，**禁止**吞错（空 catch、忽略返回码）、**禁止**只写日志不返回错误、**禁止**把故障降级为"警告后继续"；
- **降级必须显式**：上游产物/能力缺失时改走替代路径并继续运行，只允许在"显式写 `degraded_reason` + manifest 记录 + 不改变科学语义"三要件齐备时发生（合同 §6）；改变科学语义的降级 = 故障，必须 fail-closed；
- **节点运行日志**：节点事件经 `observability` 汇聚落 `<output_dir>/logs`（最高设计 §7.3）；节点不自行开文件写日志、不自行决定落点。

## 8. 测试与 Oracle

- DAG 依赖顺序测试（科学依赖不可变）；
- **编排连续性**：统计上下文切换次数与块重载次数，locality_first 显著优于轮转调度；同一块的连续节点在同一 worker 完成；
- **内存**：峰值 RSS 随块大小而非总数据量增长（构造超内存预算数据量验证流式性）；缓存 LRU 与字节预算生效；
- **缓存复用**：同组 Gaia 查询只发起一次外部请求（命中计数），缓存命中/失效不改变结果；
- 1 worker vs N worker 数值一致；
- 取消/checkpoint 恢复无半成品，且取消/失败路径的运行日志仍发布并登记；
- **错误上行**：每个节点的失败路径测试断言"返回稳定错误码 + CLI 退出码正确"，负例注入（吞掉错误码）必红；
- **降级显式**：构造上游产物缺失场景，断言 `degraded_reason` 落盘且 manifest 记录；注入静默回退（不写 `degraded_reason`）必红（判据 `CHK-LOG-SYS` R2）；
- 资源监控记录完整性；磁盘门测试（能红能绿）。

---

## 9. 模块名（全仓唯一）

- 模块名只有一份：**`scheduler`**（注册、资源预算、执行、取消、checkpoint；物理位
  `lib/infrastructure/scheduler`）+ **`pipeline`**（typed DAG、命名块、内存/数据管线；
  物理位 `lib/infrastructure/pipeline`），依据最高设计 §8.1（顶层结构）与 §7.1（命令树）。
- 对应登记：`docs/modules/MODULE_MAP.yaml` 条目 `id: scheduler` /
  `module_id: astrocs.infra.scheduler` / `target_dir: lib/infrastructure/scheduler`；
  `docs/plugins/00_INDEX.md` §2 第 2 列 = `scheduler`。
- `runtime` **不是模块名**，不得作为职责名/模块名引用；本页文件名 `19_runtime.md` 是
  `eng/packaging/config/config_registry.json` 与 `docs/DOCUMENT_INDEX.yaml` 登记在册的文档路径，仅作路径使用。
- `pipeline` 在 `docs/modules/MODULE_MAP.yaml` 中随 `index_module_count = 23` 的登记规模
  一并维护；本页与 `00_INDEX.md` 已覆盖其名。
