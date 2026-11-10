# Runtime / 资源账本 / Storage 修复报告（P0 全量 + 高价值 P1）

> 实施者：runtime-implementer（非独立审查者；审查结论唯一正本仍是 `docs/audit/audit-runtime.md`）。
> 分支 `rebuild/acsd-v2`，起点 HEAD `ef91befb7`。**提交状态**：实施期间 Lead 的
> `77a13c89c fix(science): close the P0 findings of the four independent audits` 把本轮的
> `src/runtime/*.cpp`、`src/resources/cpu_resources.cpp`、`src/storage/local_storage.cpp` 一并收进了提交
> （工作树与 HEAD 在这些文件上一致）；但 **`apps/cli/commands.cpp` 的 P0-3 接线仍未提交**
> （`git status` 仍显示 `M apps/cli/commands.cpp`，81 行）。在它被提交之前，HEAD 上的 CLI 不会先用用户
> `Limits` 建立 `Services`，H 仍然不生效——这是合并前必须带上的一处。写域：`src/runtime/*.cpp`、`src/resources/cpu_resources.cpp`、
> `src/storage/local_storage.cpp`、`apps/cli/{commands,main}.cpp`，外加上报文件 `docs/audit/runtime-fix-report.md`。
> 未改 `include/`、`src/workflows/`、`src/projection/`、`src/model/`、其它模块与 `src/runtime/generated/`。
> 未改任何科学容差、真值、估计量、归约次序或块边界；改动只涉及准入、计费、所有权、取消、有界性与 I/O 安全。

## 0. 结论摘要

| 级别 | 已在这份改动里关闭 | 部分关闭 | 只给补丁建议（不在写域） |
|---|---|---|---|
| P0 | P0-3、P0-4、P0-5、P0-6 | — | P0-1、P0-2 |
| P1 | P1-1、P1-2、P1-3、P1-4、P1-12、P1-13、P1-15、P1-7（`LocalStorage` 侧）、P1-8、P1-5（Storage 侧） | P1-11（Dyadic 写已可往返读的**设计已给出、未实现**） | P1-6、P1-9、P1-10、P1-14、P1-16、P1-17、P1-18、P1-7（product 侧）、P1-5（product 侧） |

一句话：**用户 `Limits` 现在就是进程账本**（CLI 建唯一 `Services`，workflow 的硬编码只能被夹紧、不能放宽），
**第二套线程池已删除**（`CpuResources` 变为无线程 facade，进程线程数 = `Limits.threads` + 2 条 I/O lane），
**broadcast 等待组取消后立即终态且有界**，**科学 bridge 的分配先 claim 后 alloc 再 commit**，
**spill 配额/活存、cleanup orphan、append 累计数、溢出检查、路径根约束、有界读、发布文件持久化**均已落地，
`close_command` 的排空判据包含在途 I/O，`command->waiting` 与 block 元数据不再无界。

验证：`cmake --build build/science -j16` 通过；冻结用例 `acsd_host_lifecycle` 66 检查 PASS、
`acsd_typed_smoke` 1255 检查 PASS；`acsd_process_geometry.overlap_m5_contracts` 仍有 **1/104 既存失败**
（"export positive: build_export [invalid_request]"），已用 **A/B 对照**证明与本轮改动无关（见 §6.6）。

## 1. 复现环境

- 构建：`cmake --build build/science -j16`（Ninja，GCC 14，Linux x86-64）。设计检查 `tools/check_design.py` 仍 PASS。
- 实验二进制：
  - `build/science/acsd`（真实 CLI）；
  - `/tmp/rt-fix/exp`（独立 C++ 探针，仅链接 `build/science/*.a` + 公共头 + `runtime/internal.hpp`，不改仓库源码；改写自审查者的 `/tmp/rt-audit/exp/exp.cpp`）；
  - `/tmp/rt-fix/ovf`（Storage 溢出/路径/有界读探针）；
  - `/tmp/rt-fix/adopt`（`make_services` 采纳/夹紧与线程数探针）。
- 日志：`/tmp/rt-fix/{exp,ovf,adopt}.out`、`/tmp/rt-fix/cli/*.events.jsonl`。
- 所有引用的 `file:line` 为**修复后工作树**的行号（`src/workflows/product.cpp` 正被 Lead 并发修改，
  凡引用该文件的条目都以"函数/语义锚点"为准，行号仅供定位）。

本轮改动清单（`git diff --stat`，只含写域内文件）：

```
 apps/cli/commands.cpp           |  81 +++++++++++
 src/resources/cpu_resources.cpp |  93 +++----------
 src/runtime/memory.cpp          |  53 +++++++-
 src/runtime/pipe.cpp            |  75 +++++++++++
 src/runtime/pool.cpp            |  83 ++++++++++--
 src/runtime/science_bridge.cpp  |  67 +++++++---
 src/runtime/storage.cpp         | 269 ++++++++++++++++++++++++++++++++++---
 src/storage/local_storage.cpp   | 290 +++++++++++++++++++++++++++++++++++-----
 8 files changed, 855 insertions(+), 156 deletions(-)
```

未改：`include/`、`src/workflows/`、`src/projection/`、`src/model/`、`src/numerics/`、`src/geometry/`、`src/sky/`、
`src/coadd/`、`src/runtime/generated/`、其它 docs（唯一新文件即本报告）。未 `git commit`/`push`。

## 2. P0

### P0-3 用户 `Limits` 进入 Runtime 账本（已关闭；workflow 侧只报告）

**不变量**：进程只有一个账本；`Limits.bytes/work/spill_bytes/threads` 是它的上限。任何 workflow/模块声明的
"上限"只能收紧它，不能放宽；H 必须真正参与 `admit`，不得被硬编码 1 GiB 顶替。

**改动**
1. `apps/cli/commands.cpp:326-345` 新增 `open_process_runtime(config)`：用 `config.resources` 建唯一
   `rt::PoolConfig`（`cpu_threads=Limits.threads`、`ceiling.host_bytes=Limits.bytes`、
   `ceiling.spill_bytes`、`max_work=Limits.work`、`scratch_root=config.base/scratch_directory`、read/write lane = 1/1）。
2. `apps/cli/commands.cpp:997/1095/1320`：三条命令在**任何 workflow 之前**创建它，并持有到命令结束；
   创建失败立即具名失败（`events.issue` + `run_finished` + `exit_code_of`），不再静默继续。
3. `src/runtime/pool.cpp:1463-1475`：`make_services` 若进程已有活跃 `Services`，**返回同一个实例**（采纳），
   不建第二个池/第二个账本。这是让 workflow 内部硬编码失效的关键：它得到的是按用户 `Limits` 建的账本。
4. `src/runtime/memory.cpp:70-95/345-351/398-404`：`make_child_scope`/`child_scope` 对超过父域的
   `ScopeLimits` 由"拒绝"改为**逐分量夹紧**（`narrow_limits`）。依据 API「Limits 是额度上限而非提前占有」：
   夹紧不授予任何超出父域的能力，且 `ledger_reserve` 仍检查每个祖先的 `ceiling`，所以硬编码的 1 GiB
   command ceiling 既不能放宽预算，也不会让整条命令在 `open_command` 处直接失败（那样低 H 实验无法进行）。
5. `src/runtime/pool.cpp:683-693`：`open_command` 把超池的 `cpu_tokens/io_slots` 夹紧到池自身，而不是报错。

**可复现实验**（`/tmp/rt-fix/adopt`，用户 Limits = 32 MiB / 1 thread / 1e7 work / 0 spill，workflow 请求 1 GiB / 2 tokens / 4e9 work）：

```
threads at start: 1
first make_services: ok=1 cpu_threads=1 max_work=10000000 host=33554432 spill=0
threads after first: 4                      # main + 1 CPU lane + 2 I/O lanes，无第二池
second make_services: ok=1 same_instance=1  # workflow 的 1GiB/2/4e9 请求被采纳而非新建
effective after adoption: cpu_threads=1 max_work=10000000 host=33554432
open_command(1GiB,2tokens,4e9work) under a 32MiB/1thread/1e7 budget: ok=1
effective command ceiling: host=33554432 spill=0 cpu=1  max_work=10000000
reserve(5.3MiB R+F) under 32MiB: ok=1        # 5.3 MiB < 32 MiB，合法
```

**真实 CLI 判别实验（F2）**：单帧 `x01-00`，同一 scientific 请求，只改 `resources.bytes`：

```
# H = 1 MiB
$ build/science/acsd normalize --json h1.config.json     # exit=4
frame_finished code=failed "R+F cannot be admitted at the declared minimum block size and placement;
                             the plan must reduce its working set or the request must fail before run"
run_finished exit_code=4 summary{published:0, unsolved:1}
# H = 256 MiB（x01 默认）
$ build/science/acsd normalize --json h256.config.json    # exit=0
product_published "the product was published at ./products/x01-00"
run_finished exit_code=0 summary{published:1, unsolved:0}
```

即审查报告的 6.2 节现象（H=1 MiB 仍以 5.3 MiB plan 成功发布）**不再成立**：低 H 现在是准入阶段的具名
`resource_exhausted`，默认 H 仍能正常发布（说明 `Limits.work=1e7` 也够：每 invoke 的 `work_upper` 只是
预留，实际消费按 `consume_work` 结算）。

**workflow 侧残留（须由 Lead 修，精确建议见 §7.1）**：`src/workflows/normalize.cpp:349-354,381-387`、
`src/workflows/normalize_runtime.hpp:303-312,341` 仍写死 1 GiB/2/4e9；`mosaic.cpp`/`export_workflow.cpp`
走 `open_workflow_runtime(...)` 默认参数。在 CLI 下它们已被夹紧（无害），但**其它调用者（contract 驱动）**
仍会得到 1 GiB 的账本。

### P0-4 broadcast 取消唤醒 + 等待组有界（已关闭）

**不变量**：取消后每个非 service 节点必须到达终态，且不得依赖"另一条无关 pipe 的事件"；停放集合必须是
有界控制窗口，超限给具名拒绝；`command->waiting` 不得无界。

**改动**（`src/runtime/pipe.cpp`）
1. `:300-329` 新增 `wake_parked_broadcasts_involving(pipe, code, reason)`：在 pipe 的取消 watcher 里把
   "涉及该 pipe 的停放组"从全局表移除并终结。每条 pipe 都在同一 `CancelState` 上注册过 watcher，
   所以任一组至少会被一次 watcher 命中。释放 `broadcast_mutex()` 后才 `complete_node`，不在锁内跑 continuation。
2. `:353-355` `cancel_waiters` 末尾调用它；
3. `:235-245` `try_fulfill_broadcasts` 对 `cancellation_requested(node->context.cancel)` 的组直接
   `cancelled`（覆盖"取消先发生、随后才停放"的竞态）；
4. `:657-664` 停放前若已取消，立即 `cancelled`，绝不入表；
5. `:696-709` 停放组上限 = `max(64, 8*P)`，超限 `resource_exhausted`（具名原因），不再无限增长。

**可复现实验（F1）**（`/tmp/rt-fix/exp` S3，两条 `max_items=1` 的 edge）：

```
broadcast key0 (both have credit): state=success
broadcast key1 (both full -> parked): state=waiting_dependencies terminal=0
P0-4 broadcast fill: parked=63 refused(resource_exhausted)=15     # 64 = max(64, 8*2) 上限精确命中
after request_cancel + 400ms: state=cancelled terminal=1          # 修复前：仍 waiting_dependencies
after request_cancel: extra groups still parked=0
after both pipes close: ok/ok parked state=cancelled
```

**P1-12**（`src/runtime/pool.cpp:854-884`）：`command->waiting` 上限 `max(64, 8*P)`；超限时释放已准入的
execution claim 并 `resource_exhausted`（"the bounded resource-wait window of the command is full"），
不再无界停放节点。

### P0-5 删除第二套线程池（已关闭）

**不变量**：进程只有唯一 CPU 池/唯一账本（SYSTEM「不再创建另一套线程池或重复内存账本」）。

**改动**（`src/resources/cpu_resources.cpp`）
1. `:35-46`：构造函数**不再创建任何 `std::thread`**；析构为空。原先按 `Limits.threads` 起的 worker 线程
   是 `rt::Services` 之外的第二套池（且 `worker_` 只服务于 `parallel_for`，而 `parallel_for` **从未有调用者**）。
2. `:110-118`：`parallel_for` 改为**调用线程内联、固定次序**执行（`(void)threads`），保留确定性归约；
   需要并行时由 `rt::Services` 的唯一 CPU lane 出租线程。
3. `reserve/consume_work/usage` 语义不变（用户额度仍校验），但它是**facade**：不产生线程、不是任何科学任务的
   准入闸门；唯一闸门是 `Services` 账本。

**证据**：`/tmp/rt-fix/adopt`：`threads at start: 1` → `after first make_services: 4`
（main + 1 CPU lane + 2 I/O lane）→ 第二次 `make_services`（workflow 的 2 tokens 请求）仍为 `4`。
真实 CLI（`resources.threads=1` 的单帧 normalize）运行期间采样
`/proc/<pid>/status: Threads: 4`（5 次采样恒定），即进程里只有按用户额度起的这 3 条 lane，无第二套池。

### P0-6 科学 bridge 计费（已关闭）

**不变量**：`claim → 物理分配 → commit`；claim 失败不得留下未计费分配；`Usage.peak_payload_bytes`
必须包含科学侧真实分配；`BridgeResources::reserve` 必须 commit。

**改动**（`src/runtime/science_bridge.cpp`）
1. `:54-105` `BridgeStorage::copy`：先 `claim_capacity`（persistent 优先、segment 兜底），**claim 成功后**才
   `std::make_shared<std::vector<std::byte>>`（`std::bad_alloc` → `resource_exhausted`，票据未 commit 自会
   归还 claimed、不污染 peak），然后置 `committed` 并 `ledger_commit`。旧顺序（先分配后 claim）被删除。
2. `:63-69`：空数组（`bytes.size()==0`）合法（OBJECTS「空数组合法」）：不申请容量、不 commit，仍然登记
   Array。这同时修掉了"Mosaic 单帧/零维系统无法保存空 singular_values"的具名失败（见 §6.5）。
3. `:218-232` `BridgeResources::reserve`：claim 后置 `committed=true` 并 `ledger_commit`（模块声明的
   workspace 就是它的实际分配，否则 `peak_allocated` 系统性缺失）。

**证据**：`/tmp/rt-fix/exp` S1 的 `after-seal ... allocated/peak` 包含 builder 与输入；`exp` 中
`read 4MiB with 1MiB arena: resource_exhausted` 仍是 P1-10 的接口缺口（见 §7.3），不是计费问题。

## 3. P1（写域内）

### P1-1 `append_async` 分块返回累计数（已关闭）

- `src/runtime/storage.cpp:1630-1645`：分块递归时**不再把两个累计值相加**，直接返回最后一个分块的累计值
  （`finish_append` 返回 `state->next_element`，即"成功返回累计元素数"）。
- 实验（F4）：`append(4MiB in 1MiB-chunks): ok elements=524288`（修复前 17039360）。

### P1-2 溢出检查（已关闭，写域内 6 处）

| 位置 | 修复 |
|---|---|
| `src/runtime/storage.cpp:1510-1521`（`begin_async`） | `total*itemsize` 用 `multiply_checked`；溢出 → `resource_exhausted`，**不建目录、不申请容量** |
| `src/runtime/storage.cpp:1157-1180`（read cache） | `total*itemsize`、`begin*itemsize`、`count*itemsize` 全部 checked，且 `byte_begin/byte_count ≤ expected` |
| `src/runtime/storage.cpp:1215-1232`（block descriptor） | `count*itemsize` checked 后才与 `raw_bytes` 比较 |
| `src/runtime/storage.cpp:786-806`（`feed_piece`） | `open_block_begin*itemsize + offset` checked，且有 `memory_offset ≤ memory_bytes` 的减法式上界（不再依赖回绕后的 `+`） |
| `src/storage/local_storage.cpp:26-63` | `shape` 乘积、`count*item` 全部 checked（旧 `count*item` 回绕可让 `bytes.size()==0` 混过尺寸校验） |
| `src/storage/local_storage.cpp:312-330` | `expected_elements*item` checked + 上界 `kMaxArrayBytes`，`reserve` 捕获 `bad_alloc` |

- 实验（`/tmp/rt-fix/ovf`）：`LocalStorage::open(shape 2^61 f64): resource_exhausted`；
  `read(2^63,2^63): invalid_request`。

### P1-3 spill 配额计费 + 活存账本（已关闭）

**不变量**：spill 是资源；无 spill 配额必须在 `begin` 拒绝；写入的 spool 文件空间在文件存在期间必须被计费；
sealed 之后随 Array 的寿命延续；abort/失败释放。

**改动**（`src/runtime/storage.cpp`）
1. `:1510-1553`：`begin_async` 统一算 checked `total_bytes`；spill 放置时 `claim_capacity(arena, {spill=total_bytes})`
   失败 → `resource_exhausted`（"the spill quota cannot cover the builder output"），并记录
   `WriterState::spill_claim/spill_reserved_bytes`（`:369-374`）。
2. `:1881-1904`：seal 成功时把该 claim **commit** 并交给 sealed Array 的 lease（`ArrayReaper` 持有，`:1920-1923`），
   所以在最后一个 Array 句柄死亡前，`allocated.spill`/`peak.spill` 一直计数。
3. `:559-577`：abort/失败走 `cleanup_writer`，claim 随 writer 释放。

- 实验（F4/P1-3，`/tmp/rt-fix/exp`）：
```
P1-3 begin(spill) with zero spill quota: resource_exhausted   # 修复前：spill=0 仍写 4 MiB 并 seal ok
P1-3 begin(spill with quota): ok
P1-3 after-spill-seal (array alive)  claimed s=0     allocated s=4194304  peak s=4194304
P1-3 immediately after release       claimed s=0     allocated s=0        peak s=4194304
```

### P1-4 `cleanup_writer` 的 `remove_all` 错误 → 具名 orphan（已关闭）

- `src/runtime/storage.cpp:473-549`：新增 `directory_bytes`（有界扫描，最多 65536 项）与 `record_orphan`
  （进程内表 + scratch root 下的 `runtime-orphans.jsonl` 一行 JSON；把 spill claim **commit 并保留**在该记录里，
  所以账本不会伪报空间已释放）；`:559-577` 的 `cleanup_writer` 检查 `error_code`，失败即记 orphan。
- 实验（F5）：
```
partial append before the failure: ok
forcing write error by chmod 0555 on /tmp/rt-fix/spool/writer-5
append with unwritable spool: io_error
seal after failed append: io_error (must not be ok)
abort after failed append: ok
P1-4 after-abort: allocated s=4194304 (orphan keeps the charge)   # 修复前：静默释放
P1-4 orphan: {"path":"/tmp/rt-fix/spool/writer-5","bytes":1048576,"reason":"Permission denied"}
```
说明：`bytes` 记录实际残留（1 MiB），账本保留的是声明容量（4 MiB）——**保守方向**（宁可多计，不少计），
已在 §5 列为已知保守项。

### P1-7 产品 `path` 根约束（`LocalStorage` 侧已关闭；product 侧仅报告）

- `src/storage/local_storage.cpp:71-107` `resolve_within`：拒绝绝对路径、`has_root_name/directory`、任何 `..`
  分量，并对 `base` 做逐分量的词法前缀校验；`:353` 在打开文件**之前**校验。
- 实验（`/tmp/rt-fix/ovf`）：
```
open with blocks/good.bin: ok
open with ../secret.bin: invalid_request
open with /etc/acsd-outside.bin: invalid_request
open with blocks/../../secret.bin: invalid_request
```
- 产品发布/重开侧（`src/workflows/product.cpp:354,1020`）同类校验须由 product 负责人补（§7.4）。

### P1-8 `read_file`/解压上界（已关闭）

- `src/storage/local_storage.cpp:130-165` `read_file_bounded(path, bytes, expected_bytes, error)`：先 `file_size`
  必须**恰等于**声明字节数，再一次性 `assign`（捕获 `bad_alloc`），按精确长度读。
- `:477-492` `LocalStorage::read_file` 保留硬上限 `kMaxBlockFileBytes = 2 GiB`；
- `:353-400` `open` 按 descriptor 推导期望文件大小（raw：`raw_bytes`；zstd：`stored_bytes`），并对
  `raw_bytes == count*itemsize` 做 checked 校验；累计投影超过 `expected_bytes` 即拒绝；
- `:509-528` `decompress`：`raw_bytes` 上界 + `ZSTD_getFrameContentSize` 与声明一致性 + `bad_alloc` → 具名错误。
- 实验：`read_file of a 2.1GiB file: ok=0 error=the file ... exceeds the bounded read ceiling`；
  `open with stored_bytes=1024 for a 32 byte file: invalid_model`。

### P1-13 `close_command` 排空包含在途 I/O（已关闭，保守）

- `src/runtime/pool.cpp:1589-1601`：`drained()` 除 `command->in_flight==0` 外还要求
  `hub->io_pending==0`（`:1389-1393` 在 I/O body 真正结束后以 acq_rel 递减并 notify）。
  这样"已排空"不会早于真实写入完成；`Services::shutdown` 本来就等 `io_pending==0`，语义一致。
- 保守点：`io_pending` 是进程级（`IoRequest` 没有 command 字段），因此命令关闭会等**所有**在途 I/O；
  只会延迟、不会提前。要精确到命令需要在 `internal.hpp` 的 `IoRequest` 增加 `Id command`（§7.6）。

### P1-15 `block_index_` 生命周期（已关闭）

- `src/runtime/storage.cpp:2100-2160`：`release_ref` 在最后一个引用死亡时删除该 array 的 `block_index_`
  条目（仅当没有别的存活 array 仍列出同一 canonical block id）；`register_blocks` 先撤销旧 descriptor 集
  再登记新集；`register_array` 对**同 id 二次登记**保留第一份 bytes/lease（不再静默替换，`:2060-2088`）。
- 说明：canonical block 按内容共享，所以删除前用 `still_referenced` 扫描存活 entry。

### P1-11 Dyadic 读（**未实现**，仅给出设计）

`prepare_read`（`src/runtime/storage.cpp:1128-1133`）仍对 `itemsize==0` 返回 `unsupported`；
`LocalStorage::open` 同样。原因与设计见 §7.5：它需要一条"文本块 → `Dyadic` 元素"的异步解码阶段
（目标 buffer 计费、逐元素解析规范 JSON、块 hash 与元素数校验、按 `count` 精确切分），
属于新增子流水线而不是一行补丁；本轮选择不交付半成品（半解析器比显式 `unsupported` 更危险）。

## 4. 未修项（写域外）

| ID | 位置 | 现状 | 建议 |
|---|---|---|---|
| P0-1 | `src/workflows/product.cpp` 发布尾段（当前工作树：`manifest.assessment.complete = builder.omitted.empty()`、发布前做 `collect_object_refs` 闭包检查、`manifest.complete = true` 仍无条件） | **并发修复进行中**（Lead 已加闭包检查与 assessment 降级）；但**顶层 `manifest.complete` 仍为 true，即使 capabilities 里带 `omitted:` 且 profile 仍写 `full`** | 见 §8.5 的实测；顶层 `complete` 必须与"必需闭包全部存在"一致，否则把 profile 降为具名 `signal_only` 并传播 `omitted:` |
| P0-2 | `src/workflows/product_io.cpp:734-741` | `decode_signal_estimate` 只解 identity/unit/estimator/assessment，`noise/support/signal/numerical_*/error` 丢失 | 用 `decode_ref` 补齐全部字段，并校验同 index/unit；`noise` 缺失时显式 `noise=unknown` + capability |
| P1-6 | `src/workflows/product.cpp:85-120` | 每数组一个整块（`begin=0`），`descriptor.id` 用 array object_id | 按 `canonical_array_v1`（`2^20` 字节 / dyadic 1024 元素）切块并改用 `canonical_block_id` |
| P1-9 | `src/workflows/product_io.cpp:805-835` | 未知 kind **静默 identity**；`version` 读了不校验 | 未知 kind/version → `unsupported`，合法 signal 仍独立可读 |
| P1-10 | `include/acsd/model.hpp:99-104` | 公共 `Storage` 只有 copy/read/open，无 `read_reserved` | 增加 `read_reserved(context,array,range,capacity)`；`storage.cpp` 用既有 arena claim 实现，容量不足立即失败 |
| P1-14 | `src/workflows/normalize_runtime.hpp:60-90` | `ingest_array` 整数组 `std::vector` + `register_array(persistent=true)`，无 Lease | 用 `detail::allocate_bytes`（claim→commit）承载，或让 BlockStore 持 Lease；不要 persistent |
| P1-16 | `include/acsd/model.hpp:32-34` | `Unit` 无默认成员初始化，`ArrayLayout::unit` 默认初始化后 `unit_equal` 读未初始化值 | `struct Unit { std::string scale; std::int32_t sr_power=0, pixel_power=0, nm_power=0, linear_power=0; double multiplier=1.0; };`（或在 runtime 侧统一要求 `Unit{}`；根本修复必须在头里） |
| P1-17 | `src/workflows/normalize.cpp`（merge 处） | `Overlaps` 重建丢 status/recipe/primitives；per-pixel 累积不计账 | 已由他人在工作树修，需独立复核；累积改流式/分块 |
| P1-18 | `apps/cli/events.cpp:53,100` | `flush` 后不检查流状态，事件流写失败静默 | `if (!stream_) { failed_=true; }`，`run_finished` 前检查并令退出码失败 |
| P1-5 | `src/workflows/product.cpp` 的私有 `write_file` 与 `manifest.staging/manifest` 落盘处（函数体内联，非 `LocalStorage::write_file`） | 发布路径无 fsync/fdatasync/目录 fsync | 见 §7.2 |
| P1-7 | `src/workflows/product.cpp` 重开产品时解析 `root/entry.path` 与 `base/block.path` 的两处（`read_block`/`open` 路径构造） | 未做根内约束 | 见 §7.4 |
| P1-11 | `src/runtime/storage.cpp:1128-1133`、`src/storage/local_storage.cpp:307-311` | dyadic typed read/open 仍 `unsupported` | 见 §7.5 |

## 5. 已知保守/未证明项

1. `close_command` 等待在途 I/O 是**进程级**（`io_pending`），比"本命令"更强，可能延迟关闭。
2. orphan 记录里账本保留的是**声明容量**（spill claim），而 `bytes` 字段是实际残留；方向保守（多计空间）。
3. `BridgeResources::reserve` 把"声明 workspace"当实际分配 commit（没有 allocator hook），
   所以 `peak_payload_bytes` 现在包含模块声明的科学 workspace，但仍**不包含**模块内部 `std::vector`
   的隐藏分配；真正的 M5 封顶需要受控 allocator（EXECUTION §M5，未做）。
4. P1-5 只在 `LocalStorage::write_file` 层做 `fdatasync`+目录 `fsync`（该函数目前只被测试使用）；
   真正发布的写入在 `src/workflows/product.cpp` 内联，未改。
5. 未跑 ASan/UBSan/TSan，未做 SIGKILL/掉电注入；所有并发结论仍是代码路径推理 + 定向实验。
6. 未改 `include/`，因此 P1-16（`Unit` 默认初始化）无法在本写域根治；`src/runtime/storage.cpp:72-76` 的
   `unit_equal` 仍直接比较各字段。

## 6. F1–F11 复跑结果

| 故障 | 结论 | 证据 |
|---|---|---|
| F1 broadcast 取消后永久挂起/组无上限 | **已修** | `exp` S3：取消后 400 ms 内 `cancelled`；停放 63 + 拒绝 15（上限 64） |
| F2 config H=1 MiB 仍以 5.3 MiB plan 发布 | **已修** | CLI `h1.config.json` → exit 4 `resource_exhausted`（准入阶段）；`h256` → exit 0 已发布 |
| F3 full 产品缺 covariance 仍 complete；重开 noise=null | **未修（写域外）** | 见 §7.1/§4 P0-1、P0-2 |
| F4 `append_async` 大块返回错误累计数 | **已修** | `elements=524288`（原 17039360） |
| F5 写失败 + 只读 spool → 残留且无 orphan 记录 | **已修** | `runtime-orphans.jsonl` 写入 + 账本保留 spill 计费 |
| F6 1 MiB arena 读 4 MiB 被拒（无 `read_reserved`） | **未修（写域外）** | 需 `include/acsd/model.hpp` 增接口 |
| F7 workflow ingest 4.2 MB 而账本峰值 0 | **未修（写域外）** | `exp` S4 复现：`allocations during ingest=4194908`，账本全 0 |
| F8 默认构造 `Unit` 导致 append 被拒 | **未修（写域外）** | 需 `include/acsd/model.hpp:32-34` 默认成员初始化 |
| F9 未知 action recipe 被当 identity | **未修（写域外）** | `product_io.cpp:828-830` |
| F10 发布目录残留 `manifest.staging.json` | **未修（写域外）** | `product.cpp` 未 unlink |
| F11 无 fsync 的发布 | **部分** | `LocalStorage::write_file` 已 `fdatasync`+目录 `fsync`（`ovf`：`write_file: ok=1`）；product 发布链仍缺 |

## 7. 给 Lead / 其它 owner 的精确补丁建议

### 7.1 P0-3 收尾（workflows）与 P1-14

1. `include/acsd/model.hpp:94-96` 给 `Resources` 增加 `virtual Limits limits() const = 0;`
   （`CpuResources` 已能返回；`BridgeResources` 从 `ctx_.scope->limits` 还原）。
2. `src/workflows/normalize.cpp:349-354` 与 `:381-387` 删除写死的 `1ull<<30 / 2 / 4000000000`，改为
   `const Limits limits = context.resources.limits();` 后填 `pool.process.ceiling.host_bytes = limits.bytes;
   pool.process.max_work = limits.work; pool.cpu_threads = max<U32>(1, limits.threads);
   pool.process.ceiling.spill_bytes = limits.spill_bytes;`，command scope 同源。
3. `src/workflows/normalize_runtime.hpp:303-312` 的 `open_workflow_runtime(name)` 默认参数改为从
   `Context` 取（同 1），或删除默认参数强制调用者传入。
4. `src/workflows/normalize_runtime.hpp:60-90` `ingest_array`：
   ```cpp
   auto bytes_holder = rt::detail::allocate_bytes(runtime.command, arena, view.value->bytes.size());
   if (bytes_holder.code != Code::ok) return fail<Done>(bytes_holder.code, ...);
   std::memcpy(bytes_holder.value->values.data(), view.value->bytes.data(), view.value->bytes.size());
   auto frozen = rt::detail::freeze_bytes(std::move(*bytes_holder.value));
   runtime.services->blocks->register_array(id, alias_of(frozen), dtype, shape, lease_of(frozen), /*persistent=*/false);
   ```
   要点：`persistent=false`，Lease 随 Array 释放（否则长驻买断）。

### 7.2 P1-5 发布持久化（product.cpp）

`src/workflows/product.cpp:68-84` 的私有 `write_file` 应：
1. 写临时名 `<path>.tmp-<seq>`，`ofstream` 写完后 `flush`/`close`，再用 `::open(O_RDONLY)` + `fdatasync`；
2. `::rename(tmp, path)` 后对 `path.parent_path()` 做 `::open(O_RDONLY|O_DIRECTORY)` + `fsync`；
3. 只在 `durable==true` 时执行 2 的目录 fsync（文件 fdatasync 始终做），失败返回 `io_error`；
4. Windows 分支用 `FlushFileBuffers` + `MoveFileExW(..., MOVEFILE_WRITE_THROUGH)`；
5. `:944/:998` 写 `manifest.staging.json`/`manifest.json` 后同法刷盘，最后 `rename(staging, destination)`
   再 fsync 父目录。可直接复用 `src/storage/local_storage.cpp:109-165` 的 `sync_handle/read_file_bounded`
   思路（建议把它们提成一个 `src/storage/fs_util.hpp` 供两层共用）。

### 7.3 P1-10 `read_reserved`

```cpp
// include/acsd/model.hpp
virtual Result<ArrayView> read_reserved(Context&, ArrayRef, BlockRequest,
                                        std::span<const U64> capacity) = 0; // capacity: 复用既有 claim
```
`src/runtime/storage.cpp` 的 `read_async` 已按 arena claim 分配目标 buffer；增加一个"用调用者已 claim 的
arena 而不新 reserve"的入口即可（`prepare_read` 已接受 `state->arena`，把该 arena 从 Host 的
`ctx.progress` 换成调用者的 `Reservation` 即可）。这是纯接口 + 分支，不改科学路径。

### 7.4 P1-7 产品侧路径约束（product.cpp:354,1020）

把 `src/storage/local_storage.cpp:71-107` 的 `resolve_within` 提为公共 helper（`src/storage/path.hpp`），
在 `product.cpp` 解 `root/entry.path`、`base/block.path` 时先调用；拒绝 `..`/绝对路径，并校验
`weakly_canonical` 前缀。ORPHAN/STAGING 清理也应只在这些解析后的路径上做。

### 7.5 P1-11 Dyadic typed read 设计（`storage.cpp` + `local_storage.cpp`）

1. `prepare_read` 对 `dtype==DType::dyadic` 走新分支 `prepare_dyadic_read`：用
   `allocate<Dyadic>(task, arena, range, ...)`（`checked_mul(count,sizeof(Dyadic))`）作为目标 buffer，
   并额外 claim `min(block.raw_bytes, 8 MiB)` 的文本/解析 workspace（一个 limb 字符串的最大块）。
2. 复用现有 `submit_file_read`/`ingest_block` 的 I/O 段读文本块（有界读：`raw_bytes` 已由
   `count*itemsize` 类校验替代为"文本长度 ≤ 声明 raw_bytes"），校 `raw_sha256`。
3. 新增 `decode_dyadic_block(std::string_view text, U64 count, std::span<Dyadic> out)`：
   **严格**解析 `{"exponent2":<int>,"signed_integer":"<可选负号><十进制>"}`，要求键序/无空白/无前导零、
   `count` 精确、末尾无残留；每元素写 `Dyadic{signed_integer, exponent2}`；
   失败 → `invalid_model`（块内容不是规范编码）。
4. `LocalStorage::open` 对 dyadic 用同一解码器，`expected_bytes` 改为"block 文本字节数之和"，元素数由
   `block.count` 决定；`Adopt` 时 dtype=dyadic、bytes 用规范文本（或直接给 `std::vector<Dyadic>` 形态）。
5. 验收：±0 归一、`2^1000`/`2^-1000`、大 mantissa、1023/1024/1025 尾块、损坏长度/hash 五个反例。

### 7.6 其它

- `src/runtime/internal.hpp:253-256` 的 `IoRequest` 增加 `Id command;`，`submit_file_read`/
  `submit_write_batch` 填 `ctx.ids.command`，`close_command` 就能只等本命令的 I/O（替代当前的进程级保守判据）。
- `apps/cli/events.cpp:53,100`：`emit` 后 `if (!stream_) failed_ = true;`，`run_finished` 检查 `failed_`
  并把退出码提到 `kFailure`（不改变已写出的科学事件）。
- `include/acsd/model.hpp:32-34`：见 §4 P1-16。
- `src/workflows/product.cpp:909-910`（旧行号）把 `manifest.assessment.complete` 强制 true 的那段
  必须改成"只降不升"，并把 `builder.omitted` 变成产品 capability（`omitted:` 前缀）持久化，
  在再生时**传播**而不是丢弃。

## 8. 未决问题

1. `acsd_process_geometry.overlap_m5_contracts` 的 1/104 既存失败（"export positive: build_export
   [invalid_request]"）已用 **A/B 对照**排除本轮改动：把 `src/runtime/science_bridge.cpp` 还原为
   `git show HEAD:...` 后重建，失败逐字相同。根因在 `src/projection/export_projection.cpp:383`
   （`apply` 返回 `invalid_request`），需要 projection/workflow owner 定位；我未改该文件。
2. CLI `export` 从**单帧** mosaic 产品出发同样在该处失败（`the export row cannot be applied`）：
   当前 `validation/x01/input/cli/products` 只有 `x01-00`（其他人正在重生成多帧产品），因此
   `mosaic→export` 全链未能在本轮复跑；`mosaic`（单帧）本身已 exit 0 发布。
3. `Limits.work` 的真实消耗合计未单独测量（`Usage.work` 只在 per-invocation 里）；单帧 256 MiB
   normalize 成功说明总额 < 1e7，但若更大的帧/更多帧共用同一 `Limits.work`，会在某处开始
   `resource_exhausted`——这是**真实**账本应有的行为，是否够用取决于 `plan` 的 `work_upper` 是否诚实
   （当前 geometry 每像素声明 4096，见审查 §7.2），需 Lead 决定是提高 config 的 `work` 还是先做 §7.3 的
   cohort 化。
4. P1-3 的 spill 配额以"声明输出字节"计费；若进程只写了一部分就失败，orphan 记录按实际残留记
   `bytes`，账本按声明容量计——两者不同口径，需在观测层说明。
5. **实测当前发布产物（不是本轮改动引入，供 Lead 判断）**：本轮 H=256 MiB 单帧 normalize 发布的
   `products/x01-00/manifest.json` 为
   ```
   complete=true            (顶层)
   assessment.complete=false   science=conditional   noise=unknown
   profile=full
   capabilities=[..., "omitted:the covariance action is not reopenable: the product carries no LinearAction reader"]
   ```
   即 assessment 已正确降级（Lead 的并发修复生效），但**顶层 `complete=true` + `profile=full` 仍与
   `omitted:` 能力矛盾**（EXECUTION:141「只有 required quantities 均达标、所有必需 closed graph 存在时
   才产生 complete=true」）。同时发布目录里仍留着 `manifest.staging.json`（F10 未修）。
   这只说明"发布语义"仍未收敛，不代表本轮 Runtime 改动有回归：本轮 h256 与 h1 的差异仅为 `Limits.bytes`。
