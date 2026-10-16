# Runtime 与模块执行合同

本合同依据[系统架构](../architecture/ARCHITECTURE.md)，定义同一阶段中 Runtime、registry、模块和 RunContext 的执行责任。公开声明位于[runtime.h](../../../lib/include/acsd/core/runtime.h)、[module.h](../../../lib/include/acsd/core/module.h)与[context.h](../../../lib/include/acsd/core/context.h)，跨动态库规则见[C ABI](../api/abi/ABI.md)。

## 1. 一次阶段运行

CLI 为当前命令构造 IR、注册模块并创建 Runtime。三个命令分别拥有运行上下文，不跨阶段保留内存状态。实际入口为 `runtime_client.cpp::run_pipeline`，科学节点由构建内 ModuleRegistry 工厂装配。

生产路径调用 `create_runtime(const RuntimeResourceBudget&)`，传入有效 CPU、内存上限及来源和可选压力观测。只接收 CPU 的重载表示没有提供内存回压上限，主要供明确不启用该能力的调用使用；不能把它描述为已提供完整生产预算。

RunContext 在当前 CLI 实现中由调用方创建为局部对象，借用给 Runtime::run；不是 Runtime 在堆上创建并拥有。它的寿命覆盖同步运行及在途任务收尾。Runtime 注入共享预算和 trace 服务，节点借用同一个阶段上下文。

## 2. 实际公开接口

| 类型/接口 | 输入与返回 | 寿命、并发及阻塞 |
|---|---|---|
| create_runtime(rb) | RuntimeResourceBudget → Result<unique_ptr<Runtime>> | 调用方拥有实例；cpu_budget 非法则失败 |
| load_pipeline(ir_json, registry) | IR 文本与 registry 引用 → Result<void> | 同步解析；调用时 registry 不并发写 |
| run(ctx) | RunContext& → Result<void> | 单实例一次运行，同步阻塞至终态；调用方保证 ctx 有效 |
| cancel() noexcept | 请求协作取消 | 可从其它线程请求，幂等；不是强制终止线程 |
| inspect() | Result<string> 诊断 JSON | 不重执行科学量；按头文件声明的并发面使用 |
| node_statuses() const | 节点状态快照 | 不返回内部容器引用 |
| node_manifests() const | 各节点最近 manifest 文本 | 描述已有执行结果，不触发新执行 |
| node_trace()/trace_jsonl() | 实际 trace 快照/导出 | 当前头要求运行完成后读取 |
| trace_violations() const | 观测违规提示 | 诊断，不是新的科学审批条件 |
| set_run_id(run_id) | 当前 run 身份 | 在运行前设置 |

工厂返回调用方独占的 Runtime；registry、上下文和模块引用不能在执行期间失效。实际错误域与 CLI 映射见[日志与错误](LOG_AND_ERROR.md)，局部返回码不能直接作为进程退出码。

## 3. 模块注册和调用

ModuleRegistry 保存 descriptor，并为可执行模块登记返回 unique_ptr<IModule> 的工厂。register_module、register_factory 和 create 分别检查声明、工厂及实例创建；重复身份或缺工厂按其错误返回处理，不以登记存在推断能运行。

IModule 的实际公开成员为 descriptor、validate_config、plan、execute、inspect、last_manifest。文档中的“创建/销毁”表示工厂与 C++ 对象寿命，不是不存在的 IModule::create/destroy 函数。

```mermaid
flowchart LR
    D[descriptor 与配置] --> V[validate_config]
    V --> P[plan]
    P --> X[execute]
    X --> I[inspect / last_manifest]
```

plan 只报告工作量、估计内存、并行轴、kernel 和 worker 需求，不修改科学输入。execute 通过上下文取得预算与服务，消费明确数据并生产新对象。inspect/last_manifest 说明已发生的结果，不重复科学计算。工厂创建、调度顺序与错误收敛见[调度合同](SCHEDULER.md)。

## 4. 端口、artifact 与 typed 目标

PortDescriptor 具有 name、data_schema_id、is_input、UnitId 和 CoordinateFrame。DataArtifactDescriptor 保存 id、schema、dtype、单位、坐标、shape、invalid policy、所有权、storage 和 provenance，实际结构见[artifact.h](../../../lib/include/acsd/core/artifact.h)。

这些声明描述载荷，不自动创建模型仓。当前 RunContext::store_artifact/get_artifact 传递的是 descriptor 快照；BlockFrame 主要持有数值块。完整的跨节点 typed metadata、revision 和消费者 lease 仍需按[Normalize 数据合同](NORMALIZE_DATA.md)迁移，科学实际取数还包含工作文件。

目标对象发布后只读，绑定 run、frame occurrence、类型、revision 与准确上游版本。必要科学 cache/spill 有正式身份与生命周期；开发快照不供下游取数。对象字段和回收细节由[命名块合同](PIPELINE_BLOCK.md)及[所有权](OWNERSHIP_LIFETIME.md)拥有。

## 5. 预算与线程租约

ThreadBudget 是共享的有效 CPU 预算对象。acquire(min,max,policy) 返回不可复制、可移动的 ThreadLease：NONBLOCK 不足即空，BEST_EFFORT 可取当前正可用量，BLOCK 等待归还，调用方必须保证有可能解除等待。

有效 lease 的析构或显式 release 归还其 token，release 幂等。取消不会在任务仍访问数组时提前归还预算；任务退出和对象释放后执行 RAII。仅大小语义的兼容 make 不等于真实原子预留，不能用它伪造授权。

RunContext::acquire_lease(requested) 使用注入的预算并受派发份额约束，无预算或耗尽时返回空租约。节点怎样处理空租约必须依真实调度行为解释，不能由全局默认线程数冒充已授权并行。内存和两轴分配见[数据流](../architecture/DATA_FLOW.md)。

## 6. 服务、取消与证据

RunContext 提供日志、指标、artifact、取消 token、checkpoint 和 trace 服务；集合读取返回快照。预算和 trace 等服务应在启动阶段配置好，不能因为读写服务有锁就任意并发替换运行配置。节点/provider 归属通过线程本地当前节点信息记录，避免多个并发节点互相覆盖归属。

checkpoint 标记不等于已有持久化恢复协议；恢复能力改变执行路径，须有独立版本和一致性合同。正式日志与资源观测按[可观测性](../../detail/infrastructure/observability.md)维护，ACSD_DEV_MODE 探针是旁路。

接口验证分别检查 IR/registry 错误、真实工厂创建、预算不足、取消、异常归还、状态快照及 trace 与真实调用一致性。构建、局部测试和真实科学运行各自报告；本合同没有因接口声明而宣称这些验证已运行。实际限制集中在[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
