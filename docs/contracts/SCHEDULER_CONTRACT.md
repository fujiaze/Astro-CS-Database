# 三阶段调度器接口合同（CONTRACT-501 / RELEASE-05）

> 上游：ASTROCS_DESIGN.md §8.1（三命令独立进程、独立调度器）、§8.3（三个阶段调度器）、§9（探针驱动优化）
> 依据：ENGINEERING_SPEC.md §4.1

> ID: CONTRACT-501-SCHED  状态: FROZEN  机器 schema: `eng/contracts/schemas/scheduler_probe_event.schema.json`

## 1 总则

- 三个命令 = 三个**独立进程**，各自实例化调度器与内存管线；阶段间只通过磁盘产品 + manifest + 哈希交换；
- 调度器是阶段内的**唯一执行者**：按 DAG 调度模块、管理块生命周期、分配线程预算、处理异步预取、响应取消；
- **统一 entrypoint**：`run(stage, config_json) -> manifest_json`，失败返回非零 rc 与显式错误域；
- **统一 DAG 声明**：节点 = {id, operation, 入参块集合, 产出块集合, 消费块集合, 线程安全性}；调度器据此推导执行序与块回收点；
- **模块不私建线程池**：线程数、内存上限、队列深度一律从配置/资源门读取（ENGINEERING_SPEC §4.1）。

## 2 三阶段调度形态（冻结）

| 阶段 | 形态 | 关键机制 | 确定性要求 |
|---|---|---|---|
| normalize | **异步工作流编排** | 帧内节点按 DAG 流水；多帧并行；资源空闲时异步启动独立工作流（预取下一帧 FITS、预解析星表、预建 PSF）；星表同组查询合并 + 两级缓存 | 固定 worker 数下归约顺序冻结；1/N worker **数值等价**（异步只影响顺序，归约按帧/块 ID 排序）；容差口径见 §2.1 |
| mosaic | **空间窗口并行** | 以固定大小天球窗口（tile）为调度单元；窗口内 coverage→UPM→排异→SNR² 集成顺序**固定**；窗口间无共享可变状态；输入帧块按窗口路由（只读必要切片） | 归约顺序冻结（窗口 ID、像素序）；1/N worker 数值等价（容差口径见 §2.1） |
| export | **子块流式** | 读子块 → 投影重采样 → 写 FITS；三级有界流水线 + 背压；不整幅驻留 | 输出与整幅参考路径数值一致（TAN，容差事前冻结，口径见 §2.1） |

### 2.1 并行确定性口径（冻结）

**归约顺序冻结是结构性不变量，判据是事前冻结的浮点容差**——两者不可互换：

- **结构性不变量（必须满足，判红项）**：归约顺序按帧 ID / 块 ID / 窗口 ID / 像素序固定；跨 worker **无共享浮点累加器**；异步与工作窃取只改变执行顺序，不改变结合顺序；归约量是整数/索引/mask/计数时要求**精确一致**。
- **数值判据（浮点结果）**：1 worker 与 N worker **数值等价**，容差来源按优先级取：
  1. 模块科学页已冻结的三档（`docs/science/PHASE2_UPM.md` §7：同配置重复=位精确 + `model_hash` 逐字相同；跨 worker 数（1..N）= **1e-12 绝对容差**；跨后端等价=不允许）；
  2. 未在模块页冻结的，按 `docs/contracts/TEST_MATRIX.md` §2 通用容差规则（FP64 非归约 `rtol=1e-12, atol=1e-13×scale`；FP32 产品非归约 `rtol=5e-6, atol=1e-6×scale`；归约 `C·γ_n·Σ|terms| + atol`，`C≤4`）。
- **可满足性下限**：绝对容差只有在被比较量量级使 `atol ≥ 1 ulp(scale)` 时才可判。模块冻结的是绝对容差时，必须同时声明其**适用量级域**；超出该域按同值的相对形式判（只放宽不收紧）。低于 1 ulp 的绝对容差不可满足，不得作为门。
- **模块自有门可以更严**：由构造保证位精确的路径（固定槽位写回、per-thread 定序归并、整数索引归约）其共址测试仍可断言 bitwise——这是模块的更强保证，不违反本合同；**合同层不要求逐位一致**。
- 容差在写测试前冻结；NaN/Inf/缺失的**位置与语义**必须精确一致（`docs/contracts/TEST_MATRIX.md` §2）。

## 3 资源声明（每阶段必填）

- 线程预算、内存上限（峰值工作集）、队列深度、分块/窗口/子块大小；
- 上述参数**由配置/资源门决定**，禁止硬编码线程数与 ISA；
- 编排参数（窗口大小、预取深度、帧并发度、工作窃取策略）的**最终取值**由 PERF-501 基于探针实测数据确定，本任务只保证机制正确与探针齐全。

## 4 探针事件 schema（JSONL，随事件流落盘）

每事件一行 JSON，字段冻结：

| 字段 | 类型 | 语义 |
|---|---|---|
| `ts` | float | 单调时钟秒（进程内） |
| `stage` | enum | `normalize` / `mosaic` / `export` |
| `kind` | enum | `node_wall` / `queue_wait` / `block_birth` / `block_death` / `rss` / `io` / `worker_busy` / `cache_hit` |
| `node` | string? | 节点 ID（`node_wall`/`queue_wait`） |
| `block` | string? | 块名（`block_birth`/`block_death`） |
| `bytes` | int? | 块字节数或 I/O 字节数 |
| `value` | float | 主测度（秒 / 字节 / 比例 / 命中率） |
| `unit` | string | `s` / `B` / `1` |
| `frame_id` | uint64? | 帧身份（可缺） |
| `window_id` | uint64? | 窗口/子块身份（可缺） |
| `worker` | int? | worker 序号（`worker_busy`） |

**映射表（与 observability 现有事件流）**：本 schema 与 `eng/contracts/schemas/jsonl_event_v1.schema.json` 兼容——`ts`/`kind`/`value`/`unit` 复用其字段名；`stage`/`node`/`block`/`worker` 作为 `tags` 的等价展开。落地时由 observability 侧提供单一写出点，禁止第二份事件格式。

## 5 取消与原子性

- SIGTERM：原子落盘语义保持，退出码 9；
- 磁盘满：exit 10（保持已修语义）；
- 取消点：节点间检查；取消时不写半成品产品。

## 6 负例（必须能红）

- 模块私建线程池 ⇒ 判红；
- 1/N worker 数值差超出 §2.1 冻结容差 ⇒ 判红；归约顺序随 worker 数变化（共享浮点累加器/动态归约序）⇒ 判红；
- 探针缺 `stage`/`kind` ⇒ schema 校验判红。
