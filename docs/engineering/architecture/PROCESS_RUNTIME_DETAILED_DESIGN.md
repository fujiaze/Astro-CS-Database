# ARS 与 Process Runtime 可编码详细设计

> **目标工程设计，尚未实现的 ABI v2 和 Host 接口均以 TARGET 标识。** 适用 Issue #6 与 ARS-02～12；生产代码仍遵循 [现行 Runtime](../contracts/RUNTIME.md)、[现行 Block](../contracts/PIPELINE_BLOCK.md)、[现行 AIO](../contracts/ASYNC_IO.md) 和 [公共 ABI](../api/abi/ABI.md)。背景及跨阶段边界见 [ARS 总体架构](PROCESS_RUNTIME_ARS.md)、[实施路线](PROCESS_RUNTIME_IMPLEMENTATION.md)。科学估计量始终服从 [ACSD 最高设计](../../ACSD_DESIGN.md)与 science 分册，不由调度改变。

## 1. 对照真实生产路径的职责表

| 层 | 当前生产或公开源码事实（2026-10-16 静态核对） | TARGET 责任边界 |
|---|---|---|
| CLI/Runtime | `lib/infrastructure/cli/runtime_client.cpp::run_pipeline` 构造 IR、内建 C++ ModuleRegistry 与调用方栈上的 RunContext；`Runtime::run(ctx)` 同步 | CLI 仅构造/执行本阶段 Run，Runtime 唯一拥有动态装载、资源授权和执行控制 |
| 现行 Module | `lib/include/acsd/core/module.h`: `IModule::validate_config/plan/execute`；`module_adapters.cpp` 静态模块 | DLL 可以登记多个独立 operation；科学语义归 operation，DLL 是装载/分发单位 |
| C ABI v1 | `module_api_v1.h` 的 `acsd_module_query_v1` 返回一张 module 级表，`execute` 传 JSON manifest；`host_api_v1.h` 尚无 typed store/MemoryCredit/ticket | 平行新增 version=2 表，不原地改变 v1 布局；v1 需要显式成本可测的 shim |
| Block | `acsd/core/block_frame.h` 的 `BlockMeta`、`BlockFrame::create/consume/destroy_all`；`aio_pipeline.h` 的 `PipelineFrame` 与 `aio_frame_add_block_move` | FrameBlockStore 负责 typed data/model、真实 leases、attempt/revision、只读发布和整帧回收 |
| Artifact | `acsd/core/artifact.h` 的 DataArtifactDescriptor 是描述符；`artifact_api_v1.h` 提供 open/read_all/manifest | 内存 typed data 是真实同阶段通道，文件 artifact 是跨阶段完成产品；不以描述符当数据 |
| 资源 | `acsd/core/executor.h` 已有 CpuHeavyExecutor、IoExecutor；`memory_pressure.h` governor 使用 RSS/滞回、victim 标记，不实现原子 credits 和完整 CPU/AIO 回收证明 | 单阶段统一 CPU executor、全局 MemoryCredit、FrameTaskGroup、Host AIO admission、压力下整帧 retry |
| 发布 | `aio/product_io` 的 atomic publish 与 Python HiPS COMPLETE manifest 为已有局部机制 | Frame/Mosaic/Export 使用完整 revision manifest 的读者可见提交，不把单文件 rename 当整批原子性 |

Runtime 一次只执行 Normalize、Mosaic 或 Export 之一；跨阶段用已完成磁盘产品，不传活跃内存对象。**Frame 是所有权/可撤销事务单位；Process/Operation 是科学语义单位；WorkUnit 是 CPU 调度单位。** Mosaic global model/已合入贡献与 Export 输出事务不能伪装成 Normalize Frame。

## 2. 动态多 operation ABI v2（ARS-02）：精确函数、参数与寿命

### 2.1 安装头与协商

新建（TARGET）`lib/include/acsd/abi/{module_api_v2.h,host_api_v2.h,typed_store_v2.h}`；现行 `acsd_module_query_v1` 和 ABI v1 全部保持。入口：

```c
/* TARGET: not present in current sources */
ACSD_EXPORT acsd_status ACSD_CALL
acsd_module_query_v2(uint32_t host_abi,
                     const acsd_host_api_v2 *host,
                     const acsd_module_api_v2 **out);
```

每个 DLL 有 `module_id/semver/build_digest`，枚举多个 `(operation_id,schema_version)`。Host 按 `(module_id,operation_id)` 绑定 IR，不能把 DAG 节点当 DLL；同 module 里两个 operation 能独立配置、规划和注册。所有跨界 struct 固定 `acsd_head{uint32_t struct_size;uint32_t abi_version;}` 前缀，v2 `abi_version=2`，检查强制字段的尾部边界；未知必需能力/版本返回 `ACS_ERR_UNSUPPORTED/ACS_ERR_ABI_MISMATCH`，不读越界尾部。C11 与 C++17 ABI 测试分别编译，固定 `offsetof/sizeof`。不允许 C++ 类型、异常、原生 `bool/long/size_t`、隐式 NUL 字符串跨 DLL，UTF-8 明示字节长度；新头基础 POD 可共享布局但不将 modular v1 与 legacy `common_abi_v1.h` 在一翻译单元混 include。

### 2.2 接口签名冻结候选

```c
/* TARGET skeleton: precise shapes, to be finalized in new v2 header/layout tests.
   All new public structs start with acsd_head. */
typedef struct acsd_operation_instance_v2_s acsd_operation_instance_v2;
typedef struct acsd_view_lease_v2_s acsd_view_lease_v2;
typedef struct acsd_output_writer_v2_s acsd_output_writer_v2;
typedef struct acsd_memory_grant_v2_s acsd_memory_grant_v2;
typedef struct acsd_io_ticket_v2_s acsd_io_ticket_v2;

typedef struct acsd_call_v2 {
  acsd_head head;
  acsd_str_v1 run_id, scope_id, input_occurrence, operation_id;
  uint64_t attempt_id, task_group_id, cancel_generation;
  uint32_t worker_grant, flags;
} acsd_call_v2;
typedef struct acsd_resource_plan_v2 {
  acsd_head head;
  uint64_t work_units, read_bytes, write_bytes;
  uint64_t output_bytes, scratch_fixed_bytes, scratch_per_worker_bytes;
  uint64_t aio_hold_bytes, staging_bytes, external_peak_allowance_bytes;
  uint32_t min_workers, max_workers, max_inflight_units, estimate_kind;
  /* estimate_kind: 0=bounded,1=profile_upper,2=unknown */
} acsd_resource_plan_v2;
typedef struct acsd_work_unit_v2 {
  acsd_head head;
  uint64_t ordinal, begin, end; /* half open; axis specified by op schema */
  uint32_t axis, reserved;
} acsd_work_unit_v2;
typedef struct acsd_operation_api_v2 {
  acsd_head head;
  acsd_status (*describe)(acsd_strbuf_v1 *out_descriptor,
                          acsd_error_info_v1 *err);
  acsd_status (*validate_config)(acsd_str_v1 config, acsd_error_info_v1 *err);
  acsd_status (*plan)(const acsd_call_v2*, acsd_str_v1 input_meta_json,
                      acsd_str_v1 config, acsd_resource_plan_v2 *out,
                      acsd_error_info_v1 *err);
  acsd_status (*create)(const acsd_call_v2*, const acsd_host_api_v2*,
                        acsd_str_v1 config, acsd_operation_instance_v2 **out,
                        acsd_error_info_v1 *err);
  acsd_status (*partition)(acsd_operation_instance_v2 *, uint64_t offset,
                           acsd_work_unit_v2 *units, uint64_t capacity,
                           uint64_t *required, acsd_error_info_v1 *err);
  acsd_status (*execute_unit)(acsd_operation_instance_v2*, const acsd_call_v2*,
                              const acsd_work_unit_v2*, acsd_error_info_v1*);
  acsd_status (*reduce)(acsd_operation_instance_v2*, const acsd_call_v2*,
                        acsd_error_info_v1*);
  acsd_status (*request_cancel)(acsd_operation_instance_v2*);
  void (*destroy)(acsd_operation_instance_v2*);
} acsd_operation_api_v2;
typedef struct acsd_module_api_v2 {
  acsd_head head;
  acsd_str_v1 module_id, semver, build_digest;
  uint32_t operation_count;
  acsd_status (*operation_at)(uint32_t index, acsd_str_v1 *operation_id,
                              const acsd_operation_api_v2 **api);
} acsd_module_api_v2;
```

本片段是**拟实施 ABI**：`acsd_str_v1` 表示现有固定宽度布局约定，而不是直接混 include ABI v1；Host v2 实际头要单独冻结类型定义与函数调用约定，不应直接复制片段便宣称已构建。

`describe` 必含 typed ports/schema、单位、形状、坐标/原点、optional/status、determinism、并行轴及可能副作用；descriptor 与 module registry/IR 同一 operation 身份，输出仅小型 JSON 元数据，不承载科学大图。`plan` 只读真实输入尺寸、模型/科学配置和可查元信息，不运行科学算法、隐式读盘，不将 unknown 字节报成已界定零。`partition` 使用 probe/fill 双阶段容量协议，明确 `required` 与实际写入数，work units 完整不重叠（除声明 halo 的只读重叠）；`execute_unit` 只写独占 output slice；`reduce` 等待所有 work units 完成，按固定索引/科学归约合同合并。

`create` 成功的 opaque handle 必须由同一 module 表 `destroy`，失败清空 `*out`；同一实例默认串行，只有声明线程安全才允许并发 execute_unit，或者 Host 为分片创建隔离实例；`request_cancel` 为单向请求，返回**不表示** worker/ticket 静止。C ABI 例外均转已有 `acsd_status` 和 error domain/detail_code，成功时 `ACS_OK`，预算不足 `ACS_ERR_BUDGET/ACS_ERR_NOMEM`，数据/科学前提分开，不能用空字符串假成功。Host 及 DLL pinned 至最后一个 instance/async callback/read lease 完成；不能提前 unload，也不能跨 CRT/free 域释放。

### 2.3 Host Services v2 接口总则

只读 Host 子服务表：`typed_store`、`memory_credit`、`executor`、`aio`、`allocator`、`metrics`、`cancel`、`catalog`、`kernel_provider`、`logger`。Host 拥有内存分配、对齐、版本化 block writer/reader、CPU 工作队列与统一 AIO；module 只取得短期 borrow/grant/ticket，不私建永久 worker 池、不打开隐藏工作目录。Host 内部 API 可用 C++ RAII，在 DLL 边界必须以 typed opaque handle/POD 和同族释放函数表达。线程计数包括内核 OpenMP，配置线程数不等于实际 worker grant；v1 shim 可复制/转换但必须按 bytes 计入预算，不能冒称 zero-copy。loader 须满足 [SECURE_LOADER](../api/abi/SECURE_LOADER.md) 信任和内容哈希要求。

## 3. Typed Process IR、数据对象与 FrameBlockStore（ARS-03/04）

### 3.1 逻辑图 v2 最小结构

```json
{
  "schema": "acsd.process-ir/v2", "command": "normalize", "run_id": "R",
  "nodes": [{
    "node_id": "science-operation", "module_id": "acsd.p1.example",
    "operation_id": "compute", "operation_schema_version": 2,
    "config": {"science_config": "reference-only"},
    "inputs": [{"port": "in", "from": "upstream.out",
                "data_schema_id": "acsd.image-plane/v2",
                "unit": "declared-unit", "coordinate": "pixel-center-0",
                "required": true}],
    "outputs": [{"port": "out", "data_schema_id": "acsd.image-plane/v2"}]
  }]
}
```

以上 operation/配置为**结构说明占位**，不是现有可执行 ID 或真实科学参数。IR 编译器按 `schema→operation 能力→typed 端口兼容→唯一生产者→无环→消费者计数→合法 partition/barrier` 顺序验证。单位、像元原点、HEALPix signal leaf order/HiPS tile order、dtype、shape、坐标与 optional/status 不兼容时必须显式插入经科学批准的转换算子，否则 fail-closed；未知误差不能强制变为零。脚本/JS 只产生类型化 DAG，热像素循环在 C++ op 中；动态子图要限定类型/个数/取消/幂等与科学因果，不得替换已提交输入。

对象身份（目标）：
`ObjectKey={run_id,scope_kind,scope_id,input_occurrence,attempt_id,producer_operation,port,revision}`；
`TypedHeader={schema_id,dtype,shape,strides,unit,coordinate,pixel_origin,invalid_policy,status,reason,source_hash,config_hash,model_hash}`。Normalize frame scope 必须绑定 input occurrence 的真实内容/来源身份；`frame_id` 展示名不能替代原始 SHA/occurrence；Mosaic global model/tile 与 Export product scope 有自己的 scope_id，不可造伪 Frame。Shape/strides 逐维 checked multiply 避免整型溢出，复杂 WCS/PSF/Noise metadata 用 versioned model handle，不强制硬塞一个 F32 数组。

### 3.2 内部接口与生命周期

```cpp
// TARGET C++ internal facade, not existing functions.
class FrameBlockStore {
 public:
  Result<ReadLease> borrow(const ObjectKey&, const ExpectedType&, ConsumerId);
  Result<OutputWriter> begin_output(const ObjectKey&, TypedHeader,
                                    MemoryGrant&&);
  Result<ObjectKey> publish(OutputWriter&&, const InputRevisions&);
  Result<void> consume(ReadLease&&);
  Result<uint64_t> reclaim_attempt(ScopeId, uint64_t attempt_id);
};
```

`begin_output` 持唯一 mutable writer；输出先写私有 storage，`publish` 校验所有应写索引/状态/shape/metadata 和 source revisions 后一次 release/acquire 发布为 immutable。消费者只得 read lease，生存至 work unit/AIO ticket 完成；`pending_consumers==0 && read_leases==0 && io_pins==0` 才可释放，不能因为 node 顺序或 `BlockFrame::consume` 返回 true 就提前 free。一个旧 Block 和新 Block/scratch 可以在转换瞬间共存，已存老块不应在下一步骤双重申请 credit。copy/move 必须区分唯一 allocation owner：`aio_frame_add_block_move` 只接纳其声明的 malloc 释放域，不能把 `std::vector::data()` 直接交给 C 的 free；合法 transfer 在 Runtime 的 allocation_id 账本中更换 owner，不重复申请同一物理 RAM。

现行 `BlockFrame::create` 使用内部字节 vector 且无全局 credit/read-lease；`RunContext::store_artifact` 只是 descriptor。逐边迁移真实 payload 和消费者，不能以类型声明/已存 JSON 当完成。注意 `aio_frame_kv_get` 指针随 PipelineFrame destroy 失效；后续异步或 sidecar 所需 header 在 frame 销毁前必须复制进有独立寿命的 typed metadata。Issue #7 已报告和修复对应 UAF/LEAK，不能在文档重构中重复称它们仍未修。

## 4. MemoryCredit 原子账本与准入（ARS-05）

账本必须以单一 allocation_id 去重；区分 `live_bytes`（受管且实际驻留、含 pool/caches/AIO/staging）、`reserved_bytes`（批准但尚未分配的未来支出）、`external_guard_bytes`（不受管峰值的安全折减）。`reserve→materialize` 是原子从 reserved 转 live，不能对同一 allocation 双计；borrow/read lease 只 pin，不新增物理 RAM；若 copy 到独立 buffer 则为新 allocation。已经 free 但物理未释放的 reusable pool 按实际驻留单次计费。可收紧预算 `B_safe(t)` 来源于进程/容器/Job 限额、可用内存、第三方不受控风险与紧急余量；RSS 用于独立的保守验证，不等同于 credit。

```text
managed_live + promised_reserved <= B_safe(t)
new_transition_need =
    new_output + scratch_fixed + width*scratch_per_worker
  + new_independent_aio_buffer + new_staging + unmanaged_allowance
```

**只申请增量**；旧 input 已在 live 内，在所有 read leases 完成前继续占用；若 AIO 直接 pin 老块则不能重复加一份 old_bytes。例：4096² FP32 旧块 67,108,864 B，FP64 新块 134,217,728 B，独立 scratch=32MiB、写复制 buffer=8MiB，则过渡需要新批准 176,160,768 B，旧 64MiB 仍 live；如果 AIO 只是 pin 旧块，新批准需求减去复制的 8MiB，但旧块没有消失。示例不是任何真实 op 的内存测量。

```cpp
// TARGET host-owned, atomic all-or-nothing.
Result<MemoryGrant> reserve_bundle(OwnerKey owner,
                                  Span<const IncrementalDemand> demands);
Result<BufferAllocation> materialize(MemoryGrant&& grant, uint64_t align);
Result<void> transfer_owner(AllocationId id, OwnerKey from, OwnerKey to);
void release(BufferAllocation&& allocation) noexcept;
CreditSnapshot snapshot() const;
```

任何不可控大分配须在 plan 的 `external_peak_allowance` 保守登记；不可信 `estimate_kind=unknown` 不能按 0 执行，先选择已界定的更小 tile/width，仍未知就返回 `RESOURCE_ESTIMATE_UNKNOWN`；最小合法分片也超预算则 `RESOURCE_UNSATISFIABLE`，即便没有在途任务也不能绕过。原子 reserve 拒绝部分配额，不把某些 buffer 留在半成功 owner 下。若 `B_safe` 因外部负荷瞬时下降低于已有 live，停止新增准入、进入 CRITICAL、按实际可回滚状态尝试释放；不虚构既有内存突然变少。

保留至少一个已准入 Frame 的下一过渡推进空间：在准入更多帧之前，审视 resident frame 的 `next_transition_min` 与已承诺 demand，优先执行能释放旧块的延续工作；避免“所有帧输入都已入内存，下一 output 谁也放不下”僵局。内存不足按缩 width/并行度→缩 read/write 预取→驱逐无借用 cache→等待 resident continuation→整帧 rollback 依次尝试。**等待内存的任务放调度 ready/wait 队列，不能占满 CPU workers 阻塞等待**。

压力状态 `NORMAL→PRESSURE→CRITICAL→RECOVERY→NORMAL`：软阈值有采样持续与滞回，硬预算危险立即触发，恢复低阈值持续。策略数值只有 `eng/packaging/config/runtime_resources.json` 一处来源；现行 governor RSS 缺失时 fail-open 是目标差距。新严格安全路径遇缺探针给保守上界或明确资源错误，不以 fail-open 宣称防 OOM；用户态也无法承诺防外部进程、第三方 allocator、内核突发造成的全部 OOM。

## 5. 整帧取消、quiescence、重试与 Mosaic/Export（ARS-06）

Normalize 的 `FrameExecution={run,occurrence,frame_id,attempt_id,block_store,task_group,credits,aio_tickets,cancel_generation,revision_state}`。状态：

```text
QUEUED -> ADMITTED -> RUNNING -> STAGING -> COMMITTING -> COMMITTED
                         |         |          |
                         +---------+----------+--(not yet visible)--> CANCELLING
CANCELLING -> CPU_QUIESCED -> IO_QUIESCED -> RECLAIMED -> RETRY_QUEUED
RUNNING/STAGING -> FAILED_PERMANENT (data/science error; join and release)
COMMITTING -> COMMIT_UNCERTAIN (visible/not durable; inspect/reconcile)
```

仅未 commit 且所有影响局限于可撤销私有对象的 attempt 可作为 victim；不能将外部输入或已正式发布 science product 删除。退出必须按如下控制流：

```cpp
// TARGET orchestration; cancel flag alone never frees memory.
freeze_dispatch(frame.scope, frame.attempt_id);
frame.cancel_generation.fetch_add(1);
for (auto* op : frame.active_operations) op->request_cancel();
frame.task_group.cancel();    // wakes queued, running units exit at safe points
frame.task_group.join();      // no CPU callbacks may touch old Block
frame.aio.abort_unpublished();
frame.aio.drain_all();        // OS writes / callbacks / pins truly finished
frame.store.abort_private_writers();
auto freed = frame.store.reclaim_attempt(frame.scope, frame.attempt_id);
frame.credit_ledger.verify_released(freed);
retry_queue.push(original_occurrence, frame.attempt_id+1, narrower_plan);
```

旧 attempt late callback 必带 `attempt_id/cancel_generation` 并被拒绝进入新 attempt 的 store 或产品 revision；其正在使用的 borrowed buffer 必须等 completion，不能因为拒绝 publish 就提前释放。非资源错误保留有名失败，不无穷重算；反复被牺牲的帧获得 aging/保护与逐次缩 in-flight/window/width，达到配置的尝试上限则明确资源错误，**保持输入全集**。不能只回滚部分 WorkUnits 因为科学版本与帧过程仍是一份事务；已经完成的独立科学产品也不能回滚。

Mosaic 的 coverage、global sky/UPM、共同统计和已经合入的科学贡献归 Run/GlobalModelRevision。必须先构造 immutable global model barrier，再并行局部 tile；对不具严格补偿的全局合并，不允许 frame victim 后再重复累加；如采用幂等 contribution identity/applied-set，需证明 exactly-once 归约和顺序。Export 不造 Frame，使用 `ExportTransaction` 与 product revision 的全或无发布；tile 并行和取消不改变目标 WCS/重采样科学语义。

## 6. 异步 Astro AIO 与多文件 revision（ARS-07/08）

目标服务：

```cpp
// TARGET internal and translated to Host C ABI v2 opaque handles.
enum class IoKind { READ, WRITE, FLUSH, VERIFY, PUBLISH };
struct IoRequest {
    IoKind kind; ObjectKey source; ArtifactKey dest; ByteRange range;
    BufferLease buffer; uint64_t attempt_id;
};
Result<IoTicket> submit_async(IoRequest&& req, IoSlotGrant&& slot);
Result<IoCompletion> poll_or_wait(IoTicket&, Deadline);
Result<void> abort_unpublished(ScopeId, uint64_t attempt_id);
Result<void> drain_all(ScopeId, uint64_t attempt_id);
```

提交成功后 AIO 接管 io slot 与 buffer lease；拒绝（BACKPRESSURE/取消/错误）必须保留调用方所有权并退回 slot，无半成功 ticket。每个 ticket 有 ID、attempt、buffer allocation ID、completion/error、借用和取消状态；zero-copy 写 pin 原块并计一次 RAM，copy 出来的 I/O buffer 独立计 RAM。队列 slot 上限不是总 RAM 上限：buffer bytes+编码 scratch+排队/在飞 tickets/staging 都计入 MemoryCredit。磁盘请求由独立有限 I/O worker 或 io_uring/IOCP backend 执行，计算型压缩/校验作为有 CPU lease 的 WorkUnits，不长期占 compute worker。CFITSIO 同句柄访问按明确串行/独立 reader 约束。

慢盘 `λ_produce>λ_write` 时，有限内存 buffer `B` 约在 `B/(λ_produce−λ_write)` s 填满。调度降低生产/预读、优先不依赖该 I/O 的就绪工作；不存在可做的工作时允许 CPU 闲置，不能无界 RAM 堆积假装 100% CPU。“AIO 请求已入队”也不是“写入/校验已完成”。

**完整 revision 事务**：
1. Host 为 `(run,scope,attempt,new_revision)` 创建私有 staging。
2. 写 signal 与同版本科学 sidecar/models/metadata；所有 AIO tickets 完成，验证 shape/BUNIT/geometry、文件 sha256/size、source/revision 和要求的能力状态，精度合法 unavailable 不伪装损坏。
3. 数据 flush/fsync 到可声明的 durability，写 immutable complete manifest，tree 中记录所有实际对象与校验值。
4. 最后按旧 revision 期望值 CAS/串行切换可消费的 `current_revision` pointer；读者只使用 pointer→complete manifest，而不是路径存在、tile 存在或单文件 rename。
5. 发布未完成则旧 revision 可消费，新 staging 不能消费；pointer 可能已可见但 fsync 未证实则 `COMMIT_UNCERTAIN`，重启检验 manifest/hash/durability 后恢复，不可盲删旧或新版本。

已有 C++ `atomic_publish` 单文件接口和 Python HiPS `HipsOutputStore` 的 COMPLETE manifest 是可复用机制，但需做完整 Host service 与 CLI reader 接线；不能声称它们已经让 P1/P2/P3 天然支持批次原子性。Mosaic 最终仍只发布 signal 科学图像通道，其必要 provenance/模型以额外元数据对象引用。

## 7. Compute Executor、Planner、Benchmark 和 ARS Policy（ARS-09～12）

逻辑图为 `PlanNode(operation,typed_deps,barrier)`；执行层为 `WorkUnit(task_id,logical_index,range,attempt,locality,worker_grant,credits)`；归约仍属于 operation 而非 work unit。Host 的共享 ComputeExecutor 唯一拥有 heavy work queue 与 CPU lease；帧内行带/样本和帧间 wavefront 统一调度。嵌套在 worker 上 `enqueue 同池任务→同步等待` 易在单 worker 构成死锁；使用扁平 partition 或经证明的 cooperative await，不允许运行库私建独立常驻线程池。只有 input/version/geometry 同 scope、无共享未完成消费者、数学操作保证等价且峰值受控时才能 fuse。归约按固定索引、确定性树或科学合同限定的顺序，不受完成顺序影响。

每个 operation profile 按 `build_digest/ISA/CPU affinity/NUMA/workers/dtype/shape_band/tile_size/storage backend/filesystem` 绑定，分别测 CPU kernel、内存带宽、压缩 CPU、AIO 带宽/延迟、并发干扰、cache 和不同 worker 的扩展性；报告样本数、median、p95、保守 upper、有效域与失效条件。建议可校正估算
`T_k(w)=T_serial + max(ops/compute_rate(w),bytes/memory_bandwidth(w)) + T_sync + T_io_dependency`；
这是启发式而非严格性能/科学定理。没有可信上界就缩小可测工作单元或具名拒绝，不以中位数当 OOM 上界；机器画像绝不修改科学参数。

```text
on(event ∈ {deps_ready, cpu_done, io_done, credit_release, telemetry_tick}):
    s = consistent_snapshot(CPU, credit, IO, ready_DAG)
    if hard_memory_risk(s): stop_new_admission(); consider_safe_frame_victim()
    candidates = nodes_with_all_science_deps_and_input_revisions()
    for c in candidates:
        p = plan_using_real_metadata(c)
        w = largest_supported_workers_with_atomic_credit_bundle_and_IO_slots(p,s)
        if no feasible w: waiting_queue.push(c, reason); continue
        score[c] = critical_path_gain - predicted_memory_hold - io_backlog_penalty
                   + fairness_aging + resident_input_bonus
    while free_cpu_workers and feasible_candidates:
        atomic_reserve(bundle + IO_slots + worker_entitlement)
        enqueue_work_units(); record(plan, grant, actual)
    update_hysteresis_and_profiles_from_observed_completions()
```

科学依赖和 credits 为**硬可执行条件**，score 仅在合法候选中排序。资源申请顺序固定/以整体非阻塞预留消除 CPU↔RAM↔IO 的 hold-and-wait；压力先完成 resident frame 下一步并释放旧块。硬阈值立即停发新任务；反复 victim 增加 aging/保护避免饥饿；CPU busy 是诊断，不能驱动不安全 overcommit。详细科学划分：Normalize 八节点/逐帧 wavefront、Mosaic 七节点全局 barrier+tile、Export 五节点 WCS/输出局部流式消费；各阶段必须与真实生产 API 一起迁移。

## 8. 纵向调用与可复现负例

**单帧正常链**：CLI 解析原始 FITS occurrence 和冻结 config→IR 通过科学类型/端口校验→Frame attempt=1→AIO 读并受管 buffer→calibration/clean 旧新块并存、publish clean 后最后 lease 回收 old→WCS/IPV（原始 FITS 没 WCS 头仍需尝试求解）→science_sources/PSF→photometry 应用一次→noise→Drizzle 同次球面精度与 signal→Writer 校验 signal/frame_science/模型同 attempt 同 revision→AIO tickets drain→complete manifest→commit。压力发生在 Drizzle 前：stop dispatch→CPU join→AIO drain→私有对象真实释放→attempt=2 从同一 occurrence 和科学参数完整重算；旧 attempt 的 callback 不可进入新 revision。

| 针对性反例 | 最低断言 |
|---|---|
| Block 2 readers＋1 AIO pin，节点消费者都“完成” | 最后 lease/ticket 释放前字节仍 live；只 free 一次 |
| 旧 64MiB、新 128MiB、scratch 32MiB、credit 余额 128MiB | 不分配超额 output；等待或缩小单元，不能提前销毁旧块 |
| 一个 worker 在 work unit 中等待同池子任务 | 不发生 executor 嵌套死锁，采用扁平 partition |
| RSS 探针缺失，某第三方计算不可界定 RAM 上限 | 有名保守准入失败或更小已界定子计划，不能 fail-open |
| 已取消 attempt=1 写 ticket 晚于 attempt=2 完成 | 旧结果不能污染新版本，旧 buffer 一直到 ticket 结束才释放 |
| Mosaic contribution 已永久累计还被选为 victim | 禁止选择/重复累加，除非证明 exactly-once 和补偿 |
| 磁盘持续低带宽，队列写满 | backpressure 生效，slot/内存不超售，可接受 CPU idle |
| pointer 未切换即崩溃 | 消费者仍读取旧 complete revision；私有 stage 不是产品 |
| pointer 已可见但目录 fsync 不确定 | COMMIT_UNCERTAIN，重启核验，不盲删 |
| worker 次序和硬件 profile 改变 | 科学输入/身份/数量相同，浮点按事前数值合同独立比对 |

## 9. 按 Issue 的真实施工定位与测试

| 工作包 | 改动目标（需按最新 main 核对） | 必需正负测试 |
|---|---|---|
| ARS-02/03 | `lib/include/acsd/abi/*_v2.h`、`lib/infrastructure/pipeline/module_loader/`、`module_ports.registry.json`、`typed_dag.py`、`runtime_client.cpp` | 两 operation 同 DLL 在生产 CLI 真调用、v1 兼容、ABI size/version 错、schema/图环/单位不符 |
| ARS-04/05 | scheduler typed store/credit 新组件，`block_frame.h`、`aio_pipeline.h` move 桥 | shape overflow、发布可见性、lease/ticket pin、并发 reserve 不超售、allocator 失败退还 |
| ARS-06 | FrameTaskGroup、`scheduler.cpp`、`memory_pressure.cpp` | cancel→join→drain→reclaim→retry，晚回调、连续 victim、公用模型不可回滚 |
| ARS-07/08 | `lib/infrastructure/aio/`、`product_io/` 的 Host service 与 reader/publisher | 慢盘写队列、I/O 失败、fsync/rename/crash、同目标并发 revision |
| ARS-09/10 | `executor.cpp`、scheduler Planner 与 WorkUnit 映射 | one-worker 嵌套、CPU lease 超售、global barrier、确定性归约 |
| ARS-11/12 | `lib/infrastructure/benchmark/backend_host/` 与 Runtime telemetry | 画像有效域、估算与实测偏差、unknown、饥饿/压力滞回、独立性能实证 |
| ARS-13～15 | `module_adapters.cpp` 的 p1/p2/p3 真实科学生产边 | 同 occurrence、参数、metadata、科学 estimate 的基线/HST 模拟/真实数据对照 |

验收要区分“声明存在/可链接、DLL 真调用、单测、全生产 E2E、独立科学验证”；C11/C++17 ABI layout、ASan/LSan、取消时序、资源竞争、真实 AIO 和崩溃恢复均需原始证据。此次是文档提交，源码/构建/负载/科学试验全部 **NOT_RUN**。

## 10. 跨文档差异处理

- `RUNTIME.md` 的 C++ `ModulePlan::estimated_memory_bytes` 是现行粗估，不满足新 bundle 原子额度；`SCHEDULER.md` 的 governor 仍有 fail-open，不能当严格安全机制。
- `PIPELINE_BLOCK.md` 的逻辑消费和 `OWNERSHIP_LIFETIME.md` 的异步借用责任必须合在真实 lease/ticket 最后消费者释放，不是“节点跑完就 free”。
- `ASYNC_IO.md` 的 `BoundedAsyncQueue<T>` 只界定 entry；MemoryCredit 还要涵盖 buffer、在飞和压缩临时内存。
- `ATOMIC_PUBLISH.md` 的单文件 rename/目录机制与多文件 revision pointer 是不同机制；产品完成与持久化分别报。
- `PUBLIC_API.md`/ABI v1 是**现有生产接口事实**；本文件 v2 是未来施工签名目标，绝不提前宣称现行 DLL 已能做到 zero-copy、多 operation 或整帧回滚。
- 优化只改变“何时/在哪计算”，不改变输入有效集合、WCS、PSF、`F_native/F_ref/F_applied,total`、noise/GLS、HiPS 阶语义、Mosaic 科学权重或单 signal 最终产品；不同设备/近波段混合仍由用户和科学合同决定。

本篇不创设审批门禁或新长期问题登记表。设计未实现的实际技术依赖继续按 [ARS 实施路线](PROCESS_RUNTIME_IMPLEMENTATION.md)和已有 GitHub ARS Issues 分派；原191篇语义审计状态在现有 `实验/documentation-refactor/REPORT.md` 实事求是维护。
