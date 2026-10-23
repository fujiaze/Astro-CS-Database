# 基建公共接口定稿

本篇是新基建的**目标接口实施规格**，属于API正本的详细附件。本文名称、字段、所有权、状态与错误规则已经确定，实施者按本篇落到公共头，不再各自设计一套Task/Pipe/registry。当前源码仍是旧声明，本轮只改文档，不能声称以下接口已经实现。不存在旧接口兼容义务。

## 文件与类型所有权

| 目标公共头 | 唯一声明 |
|---|---|
| include/acsd/runtime/types.hpp | 以下身份、资源、计划和运行配置DTO |
| include/acsd/runtime/task.hpp | Task、then、settle、when_all、submit_cpu、外部等待 |
| include/acsd/runtime/memory.hpp | Arena、Block、Buffer以及容量转交 |
| include/acsd/runtime/pipe.hpp | 有界单次投递与显式广播 |
| include/acsd/runtime/storage.hpp | 异步数组read/append/seal/abort |
| include/acsd/runtime/process.hpp | descriptor、typed binding、Host与registry |
| include/acsd/runtime/diagnostics.hpp | 以下try-only性能/快照入口及OFF零求值宏 |
| include/acsd/modules/*.hpp | 可组合计算函数；细目见[COMPUTE_MODULES](COMPUTE_MODULES.md) |
| include/acsd/contracts.hpp | 聚合入口；既有科学DTO逐步归入model/模块头，每个类型只定义一次 |

以下C++20代码块按顺序构成一份声明合同，使用现有acsd科学DTO/Result/Code；实现可用pimpl，不可改变公开行为。runtime命名空间固定为acsd::rt。异步返回Task<T>，其最终载荷是Result<T>，禁止再套Task<Result<T>>。成功空值统一用Done，不用Result<void>。

```cpp
#include <acsd/contracts.hpp>
#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
namespace acsd::rt {
using Done = std::monostate;
using TaskId = U64;
using ScopeId = U64;
struct Key { U64 epoch, cohort, stage, item; };
struct InvocationIds {
  Id run, command, process, operation, invocation;
  std::optional<Id> parent_invocation;
};
enum class MemoryKind { host, pinned_host, device };
struct ResourceVector {
  U64 host_bytes=0, pinned_bytes=0, device_bytes=0, spill_bytes=0;
  U32 cpu_tokens=0, io_slots=0;
};
struct ScopeLimits {
  ResourceVector ceiling;
  U64 max_work=0, metadata_bytes=0;
};
struct ScopeSnapshot {
  ResourceVector held, claimed, allocated, peak_allocated;
  U64 work=0, read_bytes=0, written_bytes=0, spill_written_bytes=0;
  U64 cleanup_work=0;
  U64 elapsed_ns=0;
  std::optional<U64> cpu_ns;
};
class Services;
class ScopeState;
class CancelState;
class Arena;
class Claim;
using ScopeRef=std::shared_ptr<ScopeState>;
using ArenaRef=std::shared_ptr<Arena>;
struct InvocationContext {
  std::shared_ptr<Services> services;
  ScopeRef scope;
  std::shared_ptr<CancelState> cancel;
  InvocationIds ids;
  std::optional<U32> region;
  ArenaRef persistent, progress;
};
enum class TaskState {
  waiting_dependencies, waiting_resources, ready, running, suspended,
  success, failed, cancelled
};
struct TaskSpec {
  Key key;
  ResourceVector running;
  U64 work_upper=0;
  Id semantic;
};
template<class T> class Task {
 public:
  using value_type=T;
  Task() noexcept;
  Task(Task&&) noexcept;
  Task& operator=(Task&&) noexcept;
  Task(const Task&)=delete;
  Task& operator=(const Task&)=delete;
  ~Task();
  bool valid() const noexcept;
  TaskId id() const noexcept;
  TaskState state() const noexcept;
};
template<class T> Task<T> ready(InvocationContext, Result<T>);
template<class T> Task<T> submit_cpu(
    InvocationContext, TaskSpec, std::function<Result<T>(InvocationContext)>);
template<class T, class F>
auto then(InvocationContext, Task<T>&&, F)
    -> std::invoke_result_t<F,T>;
template<class T, class F>
auto settle(InvocationContext, Task<T>&&, F)
    -> std::invoke_result_t<F,Result<T>>;
template<class T>
Task<std::vector<T>> when_all(InvocationContext, std::vector<Task<T>>);
template<class T>
Task<std::vector<Result<T>>> settle_all(
    InvocationContext, std::vector<Task<T>>);
template<class T> Result<T> wait_external(Task<T>&&);
bool cancellation_requested(const InvocationContext&) noexcept;
void request_cancel(const InvocationContext&) noexcept;
} // namespace acsd::rt
```

## Task、错误与取消的精确规则

| 操作 | 前后条件/失败 |
|---|---|
| ready | 消费一个已有Result，不运行科学计算；元数据仍计本scope |
| submit_cpu | callable只能捕获强所有权/按值数据；TaskSpec的key在invocation内唯一。传入callable的InvocationContext是本执行段task子scope，consume_work必须用它，不能捕获父ctx绕过work_reserved。结束归还未消费work_reserved，不回滚实际work。实际执行需cpu_tokens≥1，I/O等待不能放在callable中 |
| then | F必须返回Task<U>；仅code=ok且value存在才调用F。非ok原code/assessment/issues传递，不消费partial；输入Task被移动后不可再用 |
| settle | F接收完整Result<T>，包括partial/取消/失败；必须显式验证value与有效域，不得无依据把非ok变成ok。用于封存partial与清理，非自动恢复机制 |
| when_all | 所有依赖排空后才完成；全ok给同输入顺序vector。某子任务非ok不自动取消其它科学子任务，避免完成顺序改变哪些原始故障可见；可另显式request_cancel整个scope。非ok时不提供聚合value，错误按下述规则合并 |
| settle_all | 所有依赖排空后给同序Result列表，外层ok仅表示收集完成；模块检查每个子Result，不能忽略其中失败。收集元数据在启动前预留 |
| wait_external | 只能在CLI/GUI子进程入口、测试主线程或嵌入式调用线程用；在CPU/I/O worker内调用返回invalid_request，不阻塞池。消费Task，等其真实完成/清理后取Result |
| Task析构 | 只释放观察句柄，不阻塞、不自动取消、不释放仍在途的数据。scope保任务状态到结束；未观察的失败进入scope日志。禁止无限创建无人接收任务 |
| 取消 | request_cancel设置单调标志；停止新工作，唤醒所有资源/pipe等待者，保留实际I/O强引用；外部返回前排空清理。不能把cancel请求本身当底层操作已完成 |

同一invocation的最终结果在所有必要子任务排空后决定。优先级固定：协议/模型/完整性/internal/io_error硬失败 > 外部取消 > 非ok科学/预算停止 > ok；同级取最小Key的原始Issue为主因，其余按(Key,issue ordinal)保留。取消造成的派生cancelled不覆盖更早存在的真实I/O故障。仅nonconverged/precision_exhausted/resource_exhausted等API允许的partial可进入finalize；hard failure和cancelled不交付可发布value。完整科学请求达标前不得从partial升级ok。

Task通过有限状态CAS防重复完成，状态转移：依赖齐→ready，资源不足→waiting_resources；执行获取token→running；等待异步结果→suspended并释放token；回调回ready重新准入；最后一次完成到success/failed/cancelled。完成回调节点、取消通知节点在提交时预留，不在磁盘回调里临时分配内存才能报告失败。成功Result必须有value；Task<Done>成功value为Done{}。每次callable执行完立即销毁捕获，消费后的中间value清空；最终尚被观察的输出Ref除外，不能由Task控制块钉住一整次run的所有临时数组。

## 资源与数组所有权

```cpp
namespace acsd::rt {
Result<ScopeRef> child_scope(
    InvocationContext parent, Id local_id, ScopeLimits);
Result<ArenaRef> reserve(InvocationContext, ResourceVector);
Result<Done> consume_work(InvocationContext, U64 units);
ScopeSnapshot snapshot(const ScopeRef&) noexcept;
class Arena {
 public:
  ResourceVector available() const noexcept;
  Result<std::shared_ptr<Claim>> claim(ResourceVector);
  void close() noexcept;
};
template<class T> struct Block {
  BlockRequest range;
  Ref<Index> index;
  Unit unit;
  Id model_hash;
  std::span<const T> values;
  Lease lease;
};
template<class T> struct Buffer {
  Buffer()=default;
  Buffer(Buffer&&) noexcept=default;
  Buffer& operator=(Buffer&&) noexcept=default;
  Buffer(const Buffer&)=delete;
  Buffer& operator=(const Buffer&)=delete;
  BlockRequest range;
  Ref<Index> index;
  Unit unit;
  Id model_hash;
  std::span<T> values;
  Lease lease;
};
template<class T> Result<Buffer<T>> allocate(
    InvocationContext, ArenaRef, BlockRequest, Ref<Index>, Unit, Id model_hash);
template<class T> Result<Block<T>> freeze(Buffer<T>&&);
struct ArrayLayout {
  DType dtype;
  std::vector<U64> shape;
  Unit unit;
  Ref<Index> index;
};
enum class Placement { memory, spill };
struct BuildSpec {
  ArrayLayout layout;
  Placement placement;
  U64 buffer_bytes, metadata_bytes;
};
class ArrayWriter;
using WriterRef=std::shared_ptr<ArrayWriter>;
Task<WriterRef> begin_async(InvocationContext, ArenaRef, BuildSpec);
template<class T> Task<Block<T>> read_async(
    InvocationContext, ArenaRef, ArrayRef, BlockRequest);
template<class T> Task<U64> append_async(
    InvocationContext, WriterRef, Block<T>);
Task<ArrayRef> seal_async(InvocationContext, WriterRef);
Task<Done> abort_async(InvocationContext, WriterRef);
} // namespace acsd::rt
```

host_bytes含pinned子集，不与pinned_bytes相加；spill_bytes是活存配额，累计写量另计。held含已reserve未claim+实际claim；allocated只含物理存活claim。先在所有祖先原子检查held+增量≤ceiling，再整批登记；失败零副作用。child_scope只设上限，不提前占有；父快照已聚合子域，不再相加。

Arena的显式close或最后ArenaRef析构归还未claim容量，存活claim继续计费至最后Lease释放；Arena未close时释放claim回其available，close后回祖先。不可把Arena的一个观察引用析构当close；Host/区域所有者必须在结束时close。claim只取得容量票据并增加claimed，不分配字节/不更新physical peak。allocate先claim再实际分配，成功后在同一票据上commit：claimed转allocated并更新peak；allocator失败撤销票据，不增加物理peak。free先实际释放再减allocated并归还容量。票据绑定后台/arena不可伪造；Claim不可复制，最后票据观察引用销毁仅回滚未commit容量，已commit只由物理Lease释放。scope随其存活输出保持，CPU/线程token不能由Array租约保留。

Buffer可移动不可复制，单writer；freeze后原Buffer变空，禁止使用先前可变span。Block可复制，必须连Lease一起复制；只复制span跨异步边界非法。read_async持ArrayRef/源版本到read+解压+raw hash校验完成；缓存命中复用物理Lease，不重复claim，若需新copy则在调用域计费。Block.range/values长度、index与布局不一致invalid_model，不按shape相同猜同索引。

append_async一次只允许一个写请求在途；接收时验证连续range.begin=next_element、dtype/单位/index/hash来源，不保存裸借用。返回Task持Block至实际写完成；成功value是累计元素数，短写/故障令writer failed，之后只能abort。seal_async等待已接受写入、验证总元素数/规范块/hash后sealed；不足invalid_request，状态未被悄悄修复。abort_async幂等，排空在途后撤销私有块；sealed后的abort只做空操作，不能删除已交付Array。memory必须预留完整输出，spill只持buffer与元数据；具体规范块归OBJECTS。

allocate对字节乘积/对齐做检查；固定宽T的dtype映射明确，禁止reinterpret device内存为host Block。dyadic使用单独精确编码读写，由Storage流式接口处理，不假装固定itemsize。实际超计划先申请grow，不能先new后补账；资源不足保安全partial或失败，不无限阻塞。具体准入和保底容量规则只见[SCHEDULER](SCHEDULER.md)。

## Pipe与广播

```cpp
namespace acsd::rt {
enum class PipeOrder { ordered, unordered };
struct PipeSpec { U32 max_items; U64 max_queued_bytes; PipeOrder order; };
enum class EndKind { eof, failed, cancelled };
struct PipeEnd { EndKind kind; std::vector<Issue> issues; };
template<class T> using Delivery=std::variant<T,PipeEnd>;
template<class T> class Pipe {
 public:
  Task<Done> send(InvocationContext, Key, T);
  Task<Delivery<T>> receive(InvocationContext);
  Task<Done> close(InvocationContext, PipeEnd);
  U32 region_id() const noexcept;
  U32 edge_id() const noexcept;
};
struct InvocationPlan;
template<class T> Result<std::shared_ptr<Pipe<T>>> make_pipe(
    InvocationContext, Ref<InvocationPlan>, U32 region_id, U32 edge_id);
template<class T> Task<Done> broadcast(
    InvocationContext, Key, T,
    std::vector<std::shared_ptr<Pipe<T>>> outputs);
} // namespace acsd::rt
```

Pipe只有一个逻辑producer和一个逻辑consumer。多读者必须由broadcast连接多个单读pipe，不能让两个receive竞争丢失一半数据。T必须可携强所有权；Block的逻辑bytes为values字节，普通DTO需登记bounded_size，不允许无上界payload。max_items/bytes为0表示不创建可用pipe，make_pipe返回invalid_request，不解释成无限。

send按Key登记待投递意图，未获credit时不能预先产出一个未计费结果；计算生产者实际使用SCHEDULER的publish permit先取得信用再allocate/run，send只完成已取得permit的提交。为便于固定管道组合，permit由map_blocks/pipe执行器持有，不暴露给普通算法。直接send只接受已经计费且在计划内的T，credit等待状态和T也占预留窗口，不能无限提交send。

ordered返回next_required_key，失败/取消不制造缺口；unordered仅用于无需科学顺序的中间独立结果，最终reduce仍按固定key。**执行器声明frontier**：持有publication permit的map_blocks/pipe执行器在产生跨cohort或非零首item的key之前，必须调用该pipe的expect(key)（实现为`detail::pipe_expect`，只允许执行器/组合器调用）把next_required_key推进到该key；pipe据此区分“gap”与“乱序item”。不声明时，直接producer只允许从首个(epoch,cohort,stage)组的item 0连续发送；更高的key会park（有界背压，不重排、不静默丢弃），close(failed/cancelled)必须唤醒parked sender并以该终态完成。expect在closed后invalid_request；key必须单调不减，回退invalid_request。receive成功给Delivery：正常数据或显式PipeEnd；失败终态也以PipeEnd携原Issue，不伪EOF。send在closed后invalid_request；close线性化后拒新send，先排空此前已接受的items，再给终态。request_cancel可撤销尚未接受项；已实际I/O/计算仍先清理。所有等待者均被唤醒，重复close只能同一终态幂等，冲突终态invalid_request。

broadcast按输出edge_id排序，一次原子取得所有分支的credit，再共享同一只读T/Lease；不得持一个分支credit等待另一分支。一个分支不消费时背压全部必需分支；可丢弃诊断分支必须显式标non_scientific且try-copy D，不复用此科学broadcast。分支失败停止该必需cohort，按Task规则排空，不默默少样本。

## 计划、注册和Host

```cpp
namespace acsd::rt {
struct StageSpec {
  U32 id;
  std::vector<U32> predecessors;
  ResourceVector scratch, output_upper;
  U64 control_bytes, work_upper;
};
struct EdgeSpec {
  U32 id, from, to;
  U64 max_block_bytes;
  PipeSpec window;
  std::optional<U32> shared_allocation_group;
};
enum class RegionKind { stream, barrier };
enum class RegionTransfer { materialized, ordered_accumulator };
struct RegionSpec {
  U32 id;
  RegionKind kind;
  U64 cohorts;
  ResourceVector persistent, progress_floor;
  std::vector<StageSpec> stages;
  std::vector<EdgeSpec> edges;
};
struct RegionEdge {
  U32 from, to;
  RegionTransfer transfer;
  U64 accumulator_bytes, expected_contributions;
};
struct PlanBounds {
  U64 min_block_elements, max_block_elements;
  U32 max_window;
  bool allow_spill;
};
struct PlanOptions { U64 block_elements; U32 window; Placement placement; };
struct InvocationPlan {
  Id input_hash, request_hash, algorithm_hash;
  U64 epoch=0;
  PlanOptions options;
  U64 elements, block_elements, cohorts;
  ResourceVector persistent, progress_floor, elastic;
  std::vector<RegionSpec> regions;
  std::vector<RegionEdge> region_edges;
  U64 control_bytes, cleanup_bytes, cleanup_work_upper;
  bool adaptive;
};
struct TypeId { Id schema; U32 revision; };
struct WireRequest {
  Id request_id, process, operation;
  TypeId input_type;
  std::string input_json, input_objects_json;
  Id input_root, artifact_root;
  std::vector<BlockDescriptor> blocks;
};
struct WireResponse {
  Id request_id, process, operation, invocation;
  Id input_hash, request_hash;      // resolved identity of the decoded request
  std::string backend;              // actual backend, "cpu" today
  TypeId output_type;
  Code result_code;
  Assessment assessment;
  ScopeSnapshot resources;
  std::vector<Id> dependencies;
  std::optional<std::string> value_json;
  std::string objects_json;
  std::vector<BlockDescriptor> blocks;
  Lease artifact_lease;
};
struct ProbeDeclaration { Id id, kind; U32 semantic_revision; };
struct ProcessDescriptor {
  Id process_id, module, document;
  U32 semantic_revision;
  std::vector<Id> dependencies;
  std::vector<ProbeDeclaration> probes;
};
template<class In,class Out> struct Operation {
  Id operation_id;
  TypeId input_type, output_type;
  Result<Done> (*validate)(InvocationContext, Ref<In>);
  Result<PlanBounds> (*bounds)(InvocationContext, Ref<In>);
  Result<InvocationPlan> (*plan)(InvocationContext, Ref<In>, PlanOptions);
  Task<Out> (*run)(InvocationContext, Ref<In>, Ref<InvocationPlan>);
  Task<Out> (*finalize)(InvocationContext, Ref<In>, Ref<InvocationPlan>, Result<Out>);
};
class ProcessRegistration;
class Registry;
class WireObjects;
template<class T> struct TypeCodec {
  TypeId id;
  std::string_view schema_json;
  Result<T> (*decode)(InvocationContext, std::string_view, WireObjects&);
  Result<std::string> (*encode)(InvocationContext, const T&, WireObjects&);
};
class RegistryBuilder {
 public:
  template<class T> Result<Done> register_type(TypeCodec<T>);
  Result<std::shared_ptr<ProcessRegistration>> begin(ProcessDescriptor);
  template<class In,class Out>
  Result<Done> add(std::shared_ptr<ProcessRegistration>, Operation<In,Out>);
  Result<std::shared_ptr<const Registry>> freeze();
};
class ProcessHost {
 public:
  template<class In,class Out>
  Task<Out> invoke(InvocationContext, Id process, Id operation, Ref<In>);
  Task<WireResponse> invoke_wire(InvocationContext, Ref<WireRequest>);
  Task<Done> close_async();
};
} // namespace acsd::rt
```

validate/plan只读元数据，执行同步且在本scope控制预算内；不做科学预计算，不访问磁盘等事件。cold元数据先由command服务异步读成不可变Input，缺必须元数据invalid_request，不能偷换阻塞plan。validate/plan任一非ok不调用run，仍产终态/usage。

plan的resource vectors只描述容量，不占用token；cpu/io不列入长期persistent/progress_floor，运行时按TaskSpec获得。StageSpec.output_upper为每cohort新分配输出，不含复用父输入；共享allocation只能通过同group证明去重，不能按hash猜。adaptive=true仍必须给最大单步/最小cohort界，增量图先重新检查保底容量再执行。精确算法见SCHEDULER。

Host在run结束后对API允许的partial调用finalize；该hook不得重新运行主算法，不得把非ok改ok，封存失败可降低为io_error/internal。通用cleanup由Host/Storage完成，不给模块另建任意析构回调。close_async停止新invoke、请求取消并排空未提交任务、关闭writer/arena后停池；存活immutable输出只保allocator/ledger/Storage依赖，不强迫CPU线程一直活着。关闭后的新调用unsupported。

RegistryBuilder先register_type登记每个TypeId/C++类型及schema/codec，再begin每个process一次，再add其全部operation；freeze验证重复key/type/依赖环/探针归属、完整hook、schema codec和文档，再变只读。每个In/Out的TypeId与codec由模块公共声明登记，wire ID用稳定字符串+revision，不用RTTI地址或C++对象布局。构建生成唯一装配源、schema与能力表；不手写三套白名单。未freeze不能invoke，错Request类型在run前invalid_request，未注册key为unsupported。

### 引导与关闭入口

```cpp
namespace acsd::rt {
struct PoolConfig {
  ScopeLimits process;
  U32 cpu_threads, read_slots, write_slots;
  Id scratch_root;
};
Result<std::shared_ptr<Services>> make_services(PoolConfig);
Result<InvocationContext> open_command(
    std::shared_ptr<Services>, Id command_id, ScopeLimits, U32 weight=1);
Result<std::shared_ptr<ProcessHost>> make_host(
    std::shared_ptr<Services>, std::shared_ptr<const Registry>);
Task<Done> close_command(InvocationContext);
} // namespace acsd::rt
```

make_services核验非零CPU数和明确配额，创建唯一CPU/I/O服务与账本；同一进程只装配一次Services，多个Host共享该实例；第二次创建活跃独立池返回identity_conflict，不能用另一个账本绕过进程额度。open_command创建根command scope，ids中的process/operation为service.command/open，Host.invoke再创建独立invocation child并填科学process/operation；调用者不能重复使用同一个invocation ID。父command先close会请求所有子任务取消并排空，但存活immutable输出继续保账本/allocator寿命。

Host在运行region前把已准入的R/F arena放到InvocationContext.persistent/progress；控制元数据从persistent的control子额度申请。模块不得直接reserve另一份F。Task子scope和arena claims继承这个已存在的物理预留，work_reserved单独按执行段登记，不把父arena再次计入held。then/settle用户函数执行也在CPU lane，纯I/O回调只唤醒已有节点。

## 重规划、region与清理的补充合同

Host先调用bounds得到合法范围，再以max_block_elements、window=1、placement=memory调用plan；plan只求值不预留。若R+F不合预算，按SCHEDULER用新PlanOptions重新调用同一plan，不允许按比例缩旧ResourceVector。plan须在范围外invalid_request，溢出resource_exhausted。实现者不得通过未声明环境变量或全局当前块大小传候选值。

plan的stages/edges属于RegionSpec；stream是有限cohort，barrier的cohorts固定1。region_edges构成DAG；上游stream可把每cohort贡献按key提交到R内ordered_accumulator，提交完成即释放该cohort F，不等待全局barrier。下游barrier只在expected_contributions全部提交且上游EOF后ready；只读已冻结聚合结果。materialized边明确存活对象/版本，不能用临时全局变量传参数。跨cohort累加只有ordered_accumulator允许，普通pipe join仍须同key。

```cpp
namespace acsd::rt {
template<class T> Task<T> run_region(
    InvocationContext, Ref<InvocationPlan>, U32 region_id,
    std::function<Task<T>(InvocationContext region_context)>);
Task<Done> admit_epoch(InvocationContext, Ref<InvocationPlan>);
} // namespace acsd::rt
```

run_region等该region前驱完成后，从Host已准入的progress reservation拆分借用F并调用body，不再次reserve同一份F；region_context带对应region及R/F arena，body终态/清理后归还此借用。Host按最小region.id顺序安排可运行保底region，可有E时并行独立region，并行region的附加工作集必须另从E准入；不为尚未ready的barrier占用F。计划所需总保底F是可顺序执行各region的最大值，所有并行R/累加器始终另计。初期为保进展可串行region，不声称所有region必须并行。

科学迭代需要下一轮时，模块提交epoch严格加1的不可变plan，经admit_epoch校验前轮必需region全部完成、旧lease仍计费及新R/F可行，再运行同一region DAG。共享work不重置；旧公开模型不原位改写。没有新epoch不得重复运行已终态的(region_id,epoch)。有限迭代及停止仍由科学process拥有，运行时不发明GN/几何停止策略。

make_pipe必须绑定已验证plan、region_id/edge_id：Spec从EdgeSpec取，缺edge/重复绑定或ctx.region不一致invalid_request；不能按创建先后猜edge。broadcast按真实edge_id申请整组permit。stream_map_ordered只用于一个stream region，调用者显式给region_id，组合器按该region.edges生成pipe。

scope状态固定accepting→stopping→draining→closed。停止/取消/科学work耗尽时拒新science节点；预留control continuation（ready/settle/状态合并）仍可运行，只做有限控制组合，F不得直接跑像素循环或consume科学work，计算必须submit_cpu并使用其task_context。

abort/seal失败清理/close由runtime私有cleanup节点执行，service在内部取不可伪造cleanup权限，模块不提供公开submit_cleanup。cleanup仅允许排空已在途、abort/unlink/归还和记录secondary Issue，不提交新科学计算，不重置science work。其节点/缓冲在plan.cleanup_bytes中预留并计R，cleanup_work_upper按已知私有文件数/在途节点界计数；CPU/I/O仍用唯一pool token，在stopping/draining中继续准入。budget耗尽/取消也不能挡住这些清理节点。

cleanup的unlink只试一次；失败留下具名orphan记录与其磁盘占用，不伪报空间已释放。Storage可由下次显式清理回收，不能无限重试拖住退出。真实DMA/write仍须等完成，不能为了返回取消提前释放buffer。cleanup-only故障仅追加次要Issue，不参加主计算错误优先级排序，保持原科学/取消Code；实际科学读写的io_error仍按硬失败优先。close_command/Host.close_async均等待清理队列排空后closed。

wire绑定固定规则：同一 concrete provider/interface 对象若出现在多个 interface 字段，producer 必须按“字段 alias”分别 bind（不复制字节）；输出规范对象仍用 `intern<T>`；输入 interface 字段编码用 `alias_for<T>` 精确反查 `(指针, interface 类型)`，无匹配即 missing_dependency，绝不回退到 intern 制造第二身份。TypeId 的稳定文本为 `acsd.input.<InputStruct>` 与 `acsd.<OutputType>`（revision 由 TypeId.revision 表达），生成器与唯一头声明保持一一对应。

TypeCodec登记把TypeId和模板T的进程内类型cookie一一绑定；cookie只用于本进程类型核验，不进入wire/hash。相同TypeId不同T、缺schema/encode/decode、Operation的In/Out与已登记codec不一致在freeze失败。构建生成register_builtin_types(builder)，先登记共享model codec，再登记各Input wrapper；校准例显式调用该入口。WireObjects是当前请求/响应的局部引用解析表，由driver创建，保存已有Model/Array的强引用和block根；不依赖全局registry模型会话。encode/decode遵API的JSON规则、单位/index另由validate检查。

### 统一wire调用与局部对象表

```cpp
namespace acsd::rt {
class WireObjects {
 public:
  template<class T> Result<Ref<T>> resolve(Id alias_or_hash) const;
  template<class T> Result<Done> bind(Id alias, Ref<T>);
  template<class T> Result<Id> intern(Ref<T>);
};
Result<std::shared_ptr<WireObjects>> make_wire_objects(
    InvocationContext, std::shared_ptr<const Registry>, Ref<WireRequest>);
Task<Done> publish_wire_response(
    InvocationContext, Id result_path, WireResponse);
} // namespace acsd::rt
```

driver只调用invoke_wire，不按operation手写C++模板switch。RegistryBuilder.add<In,Out>同时生成一个类型擦除transport thunk：校验TypeId/schema→TypeCodec<In>.decode→同一个Host.invoke<In,Out>→settle最终Result→TypeCodec<Out>.encode及对象表导出。该thunk只搬数据，不有第二份科学算法；缺codec在freeze拒绝。make_wire_objects验证输入对象依赖DAG/别名/type/hash及blocks根，按类型cookie安全resolve/bind，未知引用missing_dependency，冲突identity_conflict，循环invalid_model；实际块按Storage预算/访问验证。

invoke_wire内部创建对象表，Task强持它及输入/输出Ref直到编码、必需文件/hash完成。对象表可被测试driver显式创建，但不能从其它request或进程全局拿“当前模型”。output的科学结果放WireResponse.result_code/assessment，普通科学失败也生成完整响应；外层Task<WireResponse>.code=ok表示协议处理完成，不能把它当科学成功。若连响应控制缓冲也无法分配/传输，外层才resource_exhausted/io_error且无value，driver使用预留最小错误包络，不伪造科学Result。

目标wire协议固定protocol=acsd.process/2，字段为request_id/process_id/operation_id/input_type/input/input_objects/input_root/blocks/artifact_root；C++的*_json是对象的规范UTF8文本，wire上input/objects仍为JSON对象而非双重编码字符串。默认单请求JSON上限16MiB并计command控制预算，大数组只能用外部blocks。响应包含resolved input/request hash、build/实际backend及WireResponse各科学字段；artifact_lease只供进程内寿命，不序列化。字段形状由同registry生成schema，旧contract信封不兼容保留。

编码时只使用该输出Ref闭包/Storage，不把设备句柄或尚未存在的文件路径写入响应。artifact_root下临时块由artifact_lease强持，publish_wire_response先完成块/hash再原子提交result_path，成功后这些文件成为明确transport产物；失败/无人消费时释放未交付的私有块。它不是publish_product，不授予科学complete标记。跨进程消费者只能读取已成功提交的结果文件，调用方不能只复制JSON字符串后丢lease却继续访问尚未提交块。

## 当前科学操作的typed输入标准

生成器公共入口如下；具体codec实现来自同一typed字段清单，不是另一个全局运行时对象表：

```cpp
namespace acsd::generated {
template<class T> rt::TypeCodec<T> type_codec();
Result<rt::Done> register_builtin_types(rt::RegistryBuilder&);
} // namespace acsd::generated
```

除下列明确例外，现有公开free函数f的process输入定义为FInput：按公开声明参数名收纳所有非Context参数，span替换为Ref<vector<T>>，对象Ref保持原类型，request保持该科学Request；输出为其Result<T>中的T。provider引用用Ref<Provider>强持，不能把引用捕获到Task中。字段不猜名、顺序不改变科学含义。属于服务或纯helper的函数不额外建立process。

- forward的两个重载分别登记forward.center0/forward.edge0；Input含wcs、对应point、request，不靠字段缺失猜重载。
- coadd的CoaddInput固定为frames、sky、selection、weights、request五字段；Selection/WeightSolution必需且身份完全匹配，不内部重算。输出CoaddResult。
- CatalogueProvider、FootprintProvider、LinearAction能力在其所属process按显式provider/action字段及实际method参数生成Input；纯Frames/Storage读块按service key调用。异步ArrayWriter/arena/指针不进科学wire。
- 校准CalibrateInput固定为raw:Ref<RawObservation>、request:Calibration，输出Ref<NativeObservation>。完整实施例见[CALIBRATION_WALKTHROUGH](CALIBRATION_WALKTHROUGH.md)。

签名/字段改变允许直接更新唯一声明和生成schema，不保旧Input/旧协议别名。新增操作必须补TypeId/codec、所属process文档、资源plan、合法/非法case；缺一项构建拒绝登记，不能等运行时才发现半个模块。

## 性能与中间数据入口

以下结构固定大小/借用元数据，仅在同步try调用期间有效；sink选中后才在D下复制。对大量模型不做隐式闭包遍历；模型快照由模块提供已在手的小型字段/原块描述，不能为采集重新算科学值或读磁盘。

```cpp
namespace acsd::rt {
struct ProbeTags {
  std::optional<std::string_view> frame, component, model_hash;
  std::optional<HealpixLeaf> leaf;
  std::optional<Box> roi;
  U64 attempt=0;
};
enum class PerfKind { span, counter, gauge };
struct PerfRecord {
  std::string_view probe_id;
  ProbeTags tags;
  PerfKind kind;
  U64 start_steady_ns=0;
  std::optional<U64> wall_ns, cpu_ns, count, bytes, span_id, parent_span_id;
  std::optional<double> gauge;
};
struct SnapshotField {
  std::string_view name, unit;
  std::variant<std::monostate,double,U64,bool,std::string_view,
               std::reference_wrapper<const ExactInterval>> value;
};
struct SnapshotView {
  std::string_view name, index_hash;
  DType dtype;
  BlockRequest range;
  std::span<const U64> shape;
  std::span<const std::byte> bytes;
  std::reference_wrapper<const Unit> unit;
  std::reference_wrapper<const Lease> source_lease;
};
struct SnapshotPayload {
  std::span<const SnapshotView> views;
  std::span<const SnapshotField> fields;
  std::string_view coordinate_space, index_hash;
};
bool probe_selected(const InvocationContext&, std::string_view, const ProbeTags&) noexcept;
void try_record(const InvocationContext&, const PerfRecord&) noexcept;
void try_snapshot(const InvocationContext&, std::string_view,
                  const ProbeTags&, const SnapshotPayload&) noexcept;
U64 allocate_span_id(const InvocationContext&) noexcept;
U64 steady_ns() noexcept;
void probe_construction_failed() noexcept;
} // namespace acsd::rt
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
#define ACSD_DEV_SNAPSHOT(ctx,id,tags,payload) do { try { \
  auto&& acsd_probe_ctx=(ctx); auto&& acsd_probe_id=(id); \
  auto&& acsd_probe_tags=(tags); \
  if(::acsd::rt::probe_selected(acsd_probe_ctx,acsd_probe_id,acsd_probe_tags)) \
    ::acsd::rt::try_snapshot(acsd_probe_ctx,acsd_probe_id,acsd_probe_tags,(payload)); \
  } catch(...) { ::acsd::rt::probe_construction_failed(); } \
} while(false)
#define ACSD_DEV_PERF(ctx,record) do { try { \
  ::acsd::rt::try_record((ctx),(record)); \
  } catch(...) { ::acsd::rt::probe_construction_failed(); } \
} while(false)
#else
#define ACSD_DEV_SNAPSHOT(ctx,id,tags,payload) do {} while(false)
#define ACSD_DEV_PERF(ctx,record) do {} while(false)
#endif
```

OFF两个宏完全消除参数，即使参数是本构建不存在的表达式也不要求其声明；CMake向所有调用目标传播统一0/1。ON snapshot先选择后构造payload，views/spans只在try_snapshot返回前借用，sink只能try-copy到D私有buffer，返回后不保科学Lease。临时payload元数据应在栈上有界构造，不能在宏内创建巨大vector绕过D。

try_record/try_snapshot包括可空sink、配额不足、异常、磁盘满等情况，均noexcept且不更改科学code；未选/未就绪/队列满/复制失败状态按API记录。try_record的record必须是已存在小型性能记录，不在表达式中做昂贵科学计算。span ID由allocate_span_id从Host同一源分配，0表示无法分配并丢样；steady_ns提供单调时钟。配额/策略/文件布局/字段编码继续由API拥有，不另起debug协议。

诊断入口借用const InvocationContext&，不复制其中长字符串；宏的ctx/id/tags/payload/record构造均在try内，构造异常只调用无分配、固定原子计数的probe_construction_failed，不重求ctx表达式。该计数在诊断summary按process全局unknown-scope丢样报告，不能虚构一个不存在的invocation。

每个SnapshotView自带name/dtype/shape/range/index/unit，value/lower/upper可为ADU而dq明确无量纲，sink不能按数组位置猜意义。所有span/引用/source_lease只同步借用，选中后在D下复制实际bytes及描述，返回后不保原Lease。PerfRecord的backend、owner/Task/thread和run/seq包络由runtime从真实ctx/Task补齐；跨线程invocation span的thread_id为null，不拿最终callback线程冒充kernel线程。start_steady_ns由span生产者填，span必须同时有wall_ns/span_id；CPU不可得null。所有最终JSON字段仍符合API定义。
