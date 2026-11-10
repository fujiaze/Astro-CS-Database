# Runtime / 资源账本 / Storage / 管道独立审查（task-6）

> 审查者：runtime-auditor（未参与 Runtime/Storage/Workflow 实现，本轮只读 `src/`，只新建本文件）。
> 依据：`docs/workplans/v2-science-engineering/engineering/11-RUNTIME-RESOURCES-STORAGE.md`、`review/13-INDEPENDENT-REVIEW-AND-EXPERIMENTS.md`、`docs/engineering/rebuild/{SYSTEM,API,OBJECTS,EXECUTION,VERIFICATION}.md`、`standards/{FOUNDATION_API,SCHEDULER,DEVELOPMENT}.md`。
> 方法：先按规范逐条重推不变量，再沿真实调用路径（CLI → workflows → ProcessHost → process adapter → science bridge → Storage）逐函数核对，最后用**实际构建**做判别性实验（见 §6）。不以“测试全绿”作结论；下面每条裁决都附 `file:line` 或可复现实验。

## 0. 结论摘要

任务要求与本文对应：① 状态机/所有权/lease → §2；② Task/Pipe 有界性/CPU token/消费者异常/取消 → §3；③ Storage begin/append/seal/abort、短写/满盘/kill/checksum/原子发布/重开/Dyadic/itemsize 溢出 → §4；④ 资源账本漏算/低 H/高水位 → §5；⑤ 实际运行证据 → §6；⑥ 逐子系统裁决 → §0 裁决表、§8 故障清单；X01 根因与最小修复方向 → §7。

总体裁决：**Runtime 骨架（Host 状态机、Task 终态、容量票据、seal 后不可变）可以保留；但“生产接线 + 取消/有界性 + 真实资源计费 + 产品闭包”四处不成立，其中产品 `complete=true` 但闭包缺失、broadcast 取消永久挂起、用户 H 完全未接线属于 P0。**

一句话根因（三处独立叠加）：

1. **用户资源额度没有进入 Runtime 账本。** CLI 建 `CpuResources` + `LocalStorage` 给科学 `Context`，workflows 另建一个 `rt::Services`，其 1 GiB / 2 token / 4e9 work 全部硬编码；`Limits.bytes` 只到前者，而后者才是真正跑科学任务的账本。于是“低 H（32/256 MiB）”在生产路径上根本不存在，实测 config H=1 MiB 仍用 5.3 MiB 的单次 plan 成功发布。
2. **有界/取消合同在 Pipe/broadcast 与 command 队列上不成立。** `broadcast` 的等待组无上限、取消不唤醒；`command->waiting` 无上限；`close_command` 只等 `in_flight`、不等在途 I/O 节点。
3. **产品发布把“闭包缺失”发布成 `profile=full, complete=true`，且重开永不恢复 covariance。** 实测 normalize 产品 manifest 里的唯一遗漏记录是一段 `omitted:` capability 字符串，`SignalEstimate.noise=null`，再从该产品 export 后连 `omitted:` 都没了。

X01 的 20 分钟级 overlap 开销**不是 Host 派发本身**：实测 Host 每次 invoke 生命周期 0.11 ms，而每像素一次 invoke 的固定代价 ≈6.1 ms（3 点拟合，残差 <0.6 s），其中 ≥98% 在该次 `compute_overlap` 的每次调用固定 setup（WCS chart/精确裁剪/identity 构造），被“每像素一次 invoke”放大 16384 倍/帧。详见 §7。

### P0（阻塞“正确/可发布”结论）

| ID | 问题 | 证据 |
|---|---|---|
| P0-1 | 产品 `manifest.assessment.complete=true` 被无条件覆盖；闭包收集失败后清空重建仍发布，`profile=full` 但 noise 闭包缺失 | `src/workflows/product.cpp:909-910`、`742-754`（HEAD 行号；工作树已有并发未提交修复，见 §1/§10）；实测 §6.5 |
| P0-2 | 重开永不恢复 `CovarianceAction`：`encode_signal_estimate` 写 `noise`，`decode_signal_estimate` 只解 identity/unit/estimator/assessment，`ProductReader::estimate` 也不补 | `src/workflows/product_io.cpp:729` vs `734-741`；`src/workflows/product.cpp:467-560`；实测 §6.5 |
| P0-3 | 用户 `Limits.bytes/work/threads/spill` 不进入 Runtime 账本（三处 workflow hardcode 1 GiB/2/4e9） | `src/workflows/normalize_runtime.hpp:303-312`、`src/workflows/normalize.cpp:349-354`、`381-387`、`src/workflows/mosaic.cpp:155`、`src/workflows/export_workflow.cpp:101`；实测 §6.2 |
| P0-4 | `broadcast` 等待组取消后不唤醒，`wait_external` 永久挂起；且 `broadcast_groups()` 无上限 | `src/runtime/pipe.cpp:52-55`、`630-642`、`280-303`（只清 pipe 内 waiter）；实测 §6.4-S3 |
| P0-5 | 进程内存在**第二套线程池/账本**（`CpuResources` workers + `rt::Services` workers），违反“唯一 CPU 池/唯一账本” | `apps/cli/commands.cpp:950,1035,1247`、`src/resources/cpu_resources.cpp:39-60`、`src/workflows/normalize_runtime.hpp:307` |
| P0-6 | `BridgeStorage::copy` 先 `std::vector` 物理分配、后 claim 容量；`BridgeResources::reserve` 只 claim 不 commit，`Usage.peak_payload_bytes` 永不包含科学侧实际分配 | `src/runtime/science_bridge.cpp:57-81`、`180-205`；`src/runtime/memory.cpp:122-137`；`src/runtime/host.cpp:182-192` |

### P1（需修，影响可靠性/科学闭包/可复现性）

| ID | 问题 | 证据 |
|---|---|---|
| P1-1 | `append_async` 分块时返回的“累计元素数”被逐层累加，524288 元素返回 17039360 | `src/runtime/storage.cpp:1467-1478`；实测 §6.4-S1 |
| P1-2 | `begin` 的 `total*itemsize`、read cache 的 `total*itemsize`/`begin*itemsize`、`feed_piece` 的 `open_block_begin*itemsize` 均未做溢出检查（OBJECTS 要求先 checked） | `src/runtime/storage.cpp:1380`、`1044-1051`、`690-691` |
| P1-3 | spill 配额从未计费：writer 只 claim host staging，`ResourceVector.spill_bytes` 不用于任何 spill；`spill` 0 配额仍写入 4 MiB 并 seal 成功；sealed spill 文件的活存空间无账本项 | `src/runtime/storage.cpp:1354-1372`、`1646-1647`；实测 §6.4-S1 |
| P1-4 | `cleanup_writer` 忽略 `remove_all` 的 error_code：写失败 + 只读 spool 目录后，4.1 MB 私有 spool 残留且无 orphan 记录（违反 FOUNDATION_API:410） | `src/runtime/storage.cpp:467-480`；实测 §6.4/§6.6 |
| P1-5 | 产品发布无任何 fsync/fdatasync/目录 fsync，`durable` 只进 configuration_hash；崩溃/掉电可留下“名字存在、内容撕裂”的 manifest/blocks | `src/workflows/product.cpp:68-84`、`944`；`grep fsync` 全仓库无命中 |
| P1-6 | 产品块不是 `canonical_array_v1`：每个数组写一个整块（`begin=0,count=全数组`），`descriptor.id` 用数组 object_id 而非 `canonical_block_id` | `src/workflows/product.cpp:85-120`、`src/runtime/storage.cpp:267-289` |
| P1-7 | 重开/发布时 path 未做根内约束：`root/entry.path`、`base/block.path` 接受 `..` 或绝对路径（绝对路径会替换根）；块读取后才用攻击者提供的 `raw_sha256` 校验 | `src/workflows/product.cpp:354`、`1020`；`src/storage/local_storage.cpp:201`、`218-226` |
| P1-8 | `LocalStorage::read_file` 无大小上限、`decompress` 按 manifest 的 `raw_bytes` 直接分配（未准入、无溢出检查），坏产品可造成无界分配/`bad_alloc` | `src/storage/local_storage.cpp:186`、`270-322` |
| P1-9 | 未知 action recipe 被静默解码成 `identity`（不是 `unsupported`），`version` 从未校验（OBJECTS:96/184 明确禁止） | `src/workflows/product_io.cpp:805-835` |
| P1-10 | `Storage.read` 必须一次性分配整个请求范围，公共 `Storage` 接口没有 `read_reserved`/begin/append/seal；1 MiB arena 读 4 MiB 直接 `resource_exhausted` | `include/acsd/model.hpp:99-104`、`src/runtime/storage.cpp:1032-1059`；实测 §6.4-S1 |
| P1-11 | Dyadic 只有写路径，`prepare_read` 与 `LocalStorage::open` 都对 dyadic 返回 unsupported；OBJECTS:264-271 要求的 typed read/重开不成立 | `src/runtime/storage.cpp:1016-1020`、`src/storage/local_storage.cpp:181-185` |
| P1-12 | `command->waiting`（资源等待）无上限；停放节点在准入前不持容量 | `src/runtime/pool.cpp:836-852`、`1079-1083` |
| P1-13 | `close_command` 的 drain 只等 `command->in_flight`，在途 I/O 节点不计入；可先报“已排空”再关 scope | `src/runtime/pool.cpp:1524-1546`、`src/runtime/storage.cpp:1136-1162` |
| P1-14 | `workflows::ingest_array` 整数组读入 `std::vector` + `register_array(..., persistent=true)` 且**无 Lease**：内存不计账、买断到 Services 死亡；实测 4.2 MB 分配、账本峰值仍 0 | `src/workflows/normalize_runtime.hpp:60-90`；实测 §6.4-S4 |
| P1-15 | `BlockStore::block_index_` 永不删除、`register_array` 静默覆盖同 id 条目；长驻 command 元数据无界增长 | `src/runtime/storage.cpp:1886-1927` |
| P1-16 | `model::Unit` 无默认成员初始化，`ArrayLayout::unit` 是默认初始化；`unit_equal` 比较未初始化整数 → 非确定性 `invalid_model`（实测有效代码被拒） | `include/acsd/model.hpp:32-34`、`src/runtime/storage.cpp:72-76`、`src/runtime/storage.cpp:1498-1501`；实测 §6.4-S1 |
| P1-17 | merge 后的 `Overlaps` 在 HEAD 里重建时丢 `status/recipe/primitives`（工作树已有他人并发修复，未提交）；per-pixel 结果在 workflow 内全量累积，内存随像素数增长且不计账 | `git show HEAD:src/workflows/normalize.cpp`（`merged->status` 默认构造）；工作树 `normalize.cpp` 未提交 diff；实测 RSS 31→44 MiB |
| P1-18 | CLI `EventWriter` 忽略 stdout 写失败（flush 后不检查流状态）：磁盘满时事件流可静默截断而退出码仍是 0 | `apps/cli/events.cpp:53`、`100` |

### P2（可留但应记录）

- `record_phase` 在 `ACSD_DEV_MODE=OFF` 仍构造 `PerfRecord`、调用 `snapshot()`（加锁）与 `steady_ns()`；每 invoke 5 次，OFF 构建仍付代价（`src/runtime/host.cpp:240-255`、`src/runtime/diagnostics.cpp:924-930` 只对 span 做了宏门）。
- workflow 为 trace 先调用一次 `plan_*`，Host 内部再算一次（`src/workflows/normalize.cpp:983-987`、`src/workflows/export_workflow.cpp:117-122`）：重复 plan，且“记录的计划”与“执行计划”可不同。
- memory placement 的 append 也累计 `written_bytes`（实测 4 MiB），未发生 Storage 写却记为写（`src/runtime/storage.cpp:1549-1561`）。
- 已发布产品目录里保留 `manifest.staging.json`（`complete=false`）与完整 manifest 并列（实测 §6.5）；不违反读取，但临时清理不幂等。
- `pipe_send_erased` 不检查取消标志：取消后的 send 仍可入队（`src/runtime/pipe.cpp:400-471`）。
- `pipe` 的停放窗口是硬编码 `2*max_items`，不由 plan 声明，且停放 payload 不计入 `queued_bytes`（`src/runtime/pipe.cpp:118-129`、`447-453`）。

### 子系统裁决表

| 子系统 | 裁决 | 说明 |
|---|---|---|
| `ProcessHost` 生命周期 | **可保留** | validate→bounds→plan→admit→run→finalize→cleanup 顺序与“只降不升/partial 保留/hard failure 不 finalize”成立（§2）；每 invoke 实测 0.11 ms |
| `Task` | **可保留** | 单终态 CAS、成功必带 value、continuation 走 control lane、worker 内 `wait_external` 拒绝；风险在调用方（§3） |
| `Services`/scheduler | **需修** | 队列上限部分成立；`command->waiting` 无界、cancel/close 语义缺口、两套池（P0-5、P1-12/13） |
| `Pipe` | **需修** | ordered frontier/EOF-gap 生效；parked 窗口未声明、broadcast 无界且取消不唤醒（P0-4） |
| `ArrayBuilder`(rt) | **需修** | 状态机/短写/满盘/seal 拒绝成立；计数 bug、溢出、spill 配额、orphan、无 fsync（P1-1..5） |
| 公共 `Storage` 接口 | **规格缺口** | 只有 copy/read/open；E02 的 begin/append/seal/read_reserved 不在科学侧公共接口中（P1-10） |
| `LocalStorage` + 产品 | **需修/部分替换** | 校验和读回可用；发布原子性/闭包/规范化/path 约束需重做（P0-1/2、P1-5..9） |
| 资源账本 | **需修** | 票据/祖先原子性成立；实际科学分配不计、ingest 不计、spill 不计、peak 只含 committed（P0-6、P1-3/14） |
| Workflows/CLI 接线 | **需修** | 硬编码额度、每像素 invoke、双 plan、事件流错误忽略（P0-3、P1-17/18） |

---

## 1. 被审对象与证据身份

- 分支 `rebuild/acsd-v2`；审查开始 HEAD `fa4649d57`，审查期间出现新提交 **`ca7de6d4d`**（2026-11-10 18:49，`docs(a0): baseline audit ...`）。
- **工作树在审查期间被其他成员并发修改**（2026-11-10 19:04–19:20：`src/workflows/{normalize,mosaic,product}.cpp`、`src/projection/export_projection.cpp`、`src/{coordinates/chart,geometry/kernel,sky/sky,sources/measure,uncertainty/api}.cpp`、`docs/audit/IMPLEMENTATION-STATUS.md`）。其中 `product.cpp` 新增了“对象引用闭包必须在 manifest 内”的检查、`normalize.cpp` 新增了 `block_size>1 → unsupported` 守卫与 geometry status/recipe 合并。**这些是并发修复，本文不计为已关闭**：其正确性需由另一名审查者按 §8 复跑。
- 本文的 `file:line` 以**已提交 HEAD `ca7de6d4d`** 为准（用 `git show HEAD:<path>` 复核）；实验证据锁定二进制 `cee8ce9a`（早于上述并发修复）。
- 实测二进制：`build/science/acsd`，`sha256=cee8ce9a441baba6a33e3157b1676ad28233d9fbbda60c03e98a637cae72ea9d`，构建时间 2026-11-10 19:05:30；全部 CLI 计时实验（§6.1–6.3）在该同一二进制上完成（运行前后 hash 一致）。
- 独立实验二进制：`/tmp/rt-audit/exp/exp.cpp`，仅链接 `build/science/*.a` 与公共头 + `src/runtime/internal.hpp`（读取内部状态用），**不修改仓库任何文件**。
- 实验工作目录：`/tmp/rt-audit/{x01a,x01b,x01c,x01d,exp}`；X01 输入来自 `validation/x01/input/cli`（冻结帧 `frame_00_A.fits`，128×128，f64）。

## 2. 状态机、所有权与 lease

### 2.1 实际调用路径（证据）

`acsd normalize` → `run_normalize`（`apps/cli/commands.cpp:1001`）→ `normalize(...)`（`src/workflows/normalize.cpp`）→ `ProcessHost::invoke`（`include/acsd/runtime/process.hpp:352-376`）→ `host_invoke_erased_observed`（`src/runtime/host.cpp:337-654`）：

| 阶段 | 代码 | 成立性 |
|---|---|---|
| resolve | registry `(process,operation)` 查表 + type cookie 比对（`host.cpp:353-371`） | 成立；未知 key `unsupported`，错类型 `invalid_request` |
| child scope | `rt::child_scope(ctx, invocation, remaining_limits(ctx.scope))`（`host.cpp:410-418`、`201-225`） | 成立（只收紧上限，不预先占有） |
| validate | `operation->validate` 同步、无副作用（`host.cpp:437-440`） | 成立 |
| bounds/plan | `bounds` → `plan(options{max_block,1,memory})`；R+F 不足时按 halving → spill 重规划，最多 256 次（`host.cpp:444-538`） | 顺序成立；但 `plan` 由 workflow 与 Host 各算一次（P2） |
| admit | `rt::reserve(persistent)` + `rt::reserve(progress_floor)`（`host.cpp:486-520`） | 只 reserve（held），真正 claim 在 task 执行段 |
| run | `operation->run` 返回 Task（`host.cpp:549-561`） | 成立；adapter 内 `run_operation` 只提交**一个** `submit_cpu`（`src/geometry/overlap_process.cpp:274-292`） |
| finalize | 仅 ok/nonconverged/precision_exhausted/resource_exhausted 进入；seal 失败降码且不发布 value；finalize 不得把非 ok 升为 ok（`host.cpp:194-197`、`568-640`） | 成立 |
| cleanup | `finish_invocation`: `persistent->close()`、`progress->close()`、`close_scope`；存活输出保 Lease（`host.cpp:271-278`、`296-314`） | 成立；Arena close 只退未 claim 部分（`src/runtime/memory.cpp:453-467`） |

**“未 seal 不发布”**：在 Array 层成立（`seal_async` 建 `ArrayRef`，`freeze` 后不可写，`storage.cpp:1587-1772`）；在**产品层不成立**：`publish_graph` 把 `manifest.assessment.complete` 强制置 true 并发布（P0-1），尽管 `collect_signal` 失败被降级为 `builder.omitted`。

**“借用未完成不释放”**：Array/Block 的 Lease 强持 `Allocation`+`ClaimImpl`（`memory.cpp:226-255`、`storage.cpp:1704-1713`），最后引用死亡才 commit→release，成立。**但**：`close_command` 的“排空”判据不含在途 I/O 节点（P1-13），所以“命令已完成”的账目可以早于真实写入完成；进程退出时 `Services::shutdown` 会 join I/O worker 并排空（`pool.cpp:546-650`），因此不会 UAF，但会误导上层以为写完即可发布。

**“有 credits 才开工”**：
- 容量：`admit_task` 从 `progress/persistent` claim（`pool.cpp:343-429`），`never-allocate-before-reserve` 在这个层面成立；**但**科学 bridge 绕过它（P0-6）。
- CPU token：`submit_cpu` 要求 `cpu_tokens≥1`，池按 command ceiling 发放（`pool.cpp:982-1188`）；等待资源时 `waiting_resources` 不持 token（`pool.cpp:836-852`），成立。
- work：`admit_work` 先在所有祖先检查 `work+reserved+units≤max_work`（`memory.cpp:157-180`），成立。

**取消/异常/低内存路径**：
- 取消：`request_cancel` 置标志并 `notify_capacity`（`pool.cpp:1384-1394`）；waiting/ready/deferred 节点被 `cancel_pending_node` 终结并归还 token/claim（`pool.cpp:192-216`）；`service.*` 清理节点豁免取消（`pool.cpp:51-53`、`task.cpp:324-334`）。**缺口**：broadcast 停放组不在任何唤醒路径上（P0-4）。
- 异常：`run`/`finalize` 的 C++ 异常被 `host.cpp:543-547/553-557/580-586` 捕获并降为 `internal`，不发布 value，成立。
- 低内存：Host 的重规划顺序符合 SCHEDULER；但**用户 H 根本没进来**（P0-3），所以“低 H → spill/明确拒绝”在生产路径上不可达。

## 3. Task 与 Pipe

### 3.1 有界性

| 队列/窗口 | 上限 | 证据 | 裁决 |
|---|---|---|---|
| ready window | `2P` | `pool.cpp:491`、`place_admitted_node:240-251` | 成立；超出进 deferred，超 deferred 拒绝 |
| deferred window | `max(4,8P)` | `pool.cpp:492`、`253-269` | 成立 |
| control lane | `max(64,8P)`，溢出在完成线程内联执行 | `pool.cpp:493`、`904-918` | 有界；但“内联执行 completion”可能让 I/O 线程跑回调（回调应短，风险留给调用方） |
| `command->waiting` | **无上限** | `pool.cpp:836-852`、`1079-1083` | **不成立**（P1-12） |
| pipe ready 队列 | `max_items`/`max_queued_bytes` | `pipe.cpp:118-122` | 成立 |
| pipe 停放窗口 | 硬编码 `2*max_items`，且 payload 不计 `queued_bytes` | `pipe.cpp:123-129`、`447-453` | 未按 plan 声明（P2） |
| broadcast 停放组 | **无上限**（进程级 `std::vector`） | `pipe.cpp:52-55`、`635` | **不成立**（P0-4） |
| bindings | 进程级 map，弱引用、碰撞时清理 | `pipe.cpp:59-85` | 有界于存活 plan 数，可接受 |

实测 S2：`max_items=1` 时
```
send item=0 -> state=success
send item=1 -> state=waiting_dependencies
send item=2 -> state=waiting_dependencies
send item=3 -> state=failed        (resource_exhausted: "the bounded pipe reorder window is full")
```
即声明 `max_items=1`，实际同时持有 3 个 payload（1 个已入队 + 2 个停放），停放窗口是未声明的 2 倍。

### 3.2 等待 I/O 是否持 CPU token

**不持**。`read_async` 的 I/O 段由 `submit_file_read` 直接 `make_node`+`submit_io`，不带 `TaskSpec`/CPU token（`src/runtime/storage.cpp:1136-1162`）；`Services::io_loop` 只执行阻塞 body（`pool.cpp:1329-1357`）；压缩/编码/校验在独立 `submit_cpu` 段（`storage.cpp:1249-1281`、`1534-1539`）。这一条**成立**，是本轮少数完全符合 EXECUTION:159 的部分。

### 3.3 消费者异常是否停生产者

只在**显式 `close(failed/cancelled)`** 时成立：close 唤醒所有 parked sender（`pipe.cpp:557-569`，实测 item=2 → cancelled），并把缺 key 的 EOF 改判为 `failed`+`missing_dependency`（`pipe.cpp:542-556`，静态代码）。若消费者直接死亡而没人 close，生产者会一直产到窗口满，然后拿到 `resource_exhausted`——**有界但错误码误导**（真实原因是消费者死亡）。没有“消费者任务失败 → 自动关闭 pipe”的路径。

### 3.4 fork/join/barrier

`run_region`/`admit_epoch` 已声明（`include/acsd/runtime/process.hpp` 尾部、`host.cpp:933-1000`），`epoch` 严格 +1；但真实 workflow 没有使用 region/barrier 组合，`run_operation` 每个 operation 只发一个 task。SCHEDULER 的 S-FORK/S-BARRIER/PIPE-CHAIN 在多模块真实链上**未实现可跑路径**（规格存在、生产未用）。

### 3.5 取消是否真停底层 I/O

按合同**不要求停**：I/O body 无取消检查，节点与 buffer 继续到完成（`storage.cpp:1136-1162`）；`cancel_waiters` 只处理 pipe 等待者（`pipe.cpp:280-303`）。这符合“取消 future ≠ 底层 I/O 已停”；缺口在 §2 的 drain 判据与 P0-4。

## 4. Storage

### 4.1 `ArrayBuilder` 状态机（rt）

`open → sealed | failed → aborted`，`failed` 后 append 拒绝、seal 拒绝（`storage.cpp:1478-1531`、`1600-1626`），未 seal 析构等价 abort（`storage.cpp:382-389`），成立。**实测**（S1，无短写）：
```
begin(memory,4MiB,1MiB-chunks): ok
append(4MiB in 1MiB-chunks): ok elements=17039360      <-- 524288 期望，P1-1
seal: ok blocks=4                                      (canonical 1 MiB 块，正确)
read after seal: ok sampled_equal=1
read 4MiB with 1MiB arena: resource_exhausted          (无 read_reserved，P1-10)
```

### 4.2 短写/满盘/kill/checksum

- **满盘/短写**：`submit_write_batch` 用 `ofstream`+`flush`，失败置 `io_error`，`seal` 沿原码拒绝（`storage.cpp:922-960`、`1697-1699`）。实测（把私有 spool 目录 chmod 0555 后 append）：
```
append with unwritable spool: io_error
seal after failed append: io_error (must not be ok)
abort after failed append: ok
```
  即“永不发布半成品 complete”在 Array 层成立。
- **kill**：本次只做了“写失败”而不是 SIGKILL；从代码推断 SIGKILL 会留下 `writer-N` 私有 spool（`WriterState` 析构不会执行），没有恢复/清理入口；`cleanup_writer` 的 `remove_all` 还忽略错误（实测只读目录残留 4.1 MB，P1-4）。
- **checksum**：读路径逐块校验 `raw_sha256`（`storage.cpp:1192-1202`）与 zstd 解压长度（`1176-1190`），成立；产品 `LocalStorage::open` 也校验（`local_storage.cpp:208-226`）。发布路径**只检查文件存在**，不校验落盘内容（`product.cpp:921-933`），依赖读时暴露。
- **重复源/未知 recipe**：未知 recipe 被解码成 `identity`（P1-9），重复源/`shared_id` 冲突检查在 typed 层，不在 Storage。

### 4.3 原子发布是否包含 manifest 与依赖

顺序正确：先写 staging 块/对象 → `manifest.staging.json(complete=false)` → 存在性检查 → 写 `manifest.json(complete=true)` → `rename(staging,destination)`（`product.cpp:758-944`）。**但**：
- 全程无 fsync/fdatasync/目录 fsync（P1-5）；`durable` 只进 hash（`product.cpp:898`）。
- “依赖就绪”只是 `std::filesystem::exists`，不是 hash/尺寸校验（`product.cpp:921-933`）。
- 发布目录里残留 `manifest.staging.json`（实测 §6.5）。
- `manifest.assessment.complete` 被强制 true（P0-1）。

### 4.4 新进程重开产品的完整闭包

实测（§6.5）：从发布产品在新进程 export **成功**，signal/support/bounds/response/error 都能重开；**但**：
- `decode_signal_estimate` 不解 `signal/support/noise/numerical_*/error`（`product_io.cpp:734-741`），`ProductReader::estimate` 手工补回除 `noise` 外的字段（`product.cpp:467-560`）；`noise` 永远为 null。
- normalize 产品 `profile=full` 却带 `omitted:the covariance model is not reopenable...`；重开后再 export，新 manifest 的 capabilities 连这条 `omitted:` 都没有 → **痕迹丢失**。
- 未知 recipe → identity（P1-9），所以“未知 recipe 拒绝”不成立。
- 消费者若直接调用公开 `decode_signal_estimate` 会得到只有 identity/unit/estimator/assessment 的空壳（P2）。

### 4.5 Dyadic typed 读写

写：`append_async<Dyadic>` 已实例化，1024 元素规范块、逐元素规范编码（`storage.cpp:761-780`、`1373-1378`、`819-828`）。读：`prepare_read` 对 `itemsize==0` 直接 unsupported（`storage.cpp:1016-1020`），`LocalStorage::open` 同样拒绝（`local_storage.cpp:181-185`）。所以 **Dyadic 只能写不能读/不能重开**，OBJECTS:264-271 的 typed read 与 ST-DYADIC 门槛未满足。

### 4.6 `total*itemsize` 溢出

未检查乘法清单：`storage.cpp:1380`（begin 的 `total*itemsize`）、`1044-1051`（read cache 的 `total*itemsize`、`begin*itemsize`、`count*itemsize`）、`690-691`（`open_block_begin*itemsize + ...`）、`local_storage.cpp:186`（`expected_elements*item`）。当前 read 路径因为 `allocate<T>` 先做了 `checked_mul(count,sizeof(T))` 而未观察到越界写，但语义是错的：溢出被归类为 `internal`/错误长度，而非 OBJECTS 要求的 `invalid_request/resource_exhausted` 且“失败前不分配”。ST-OVERFLOW 不能算通过。

## 5. 资源账本

### 5.1 计费/漏算清单

| 项 | 是否计费 | 证据 |
|---|---|---|
| `allocate<T>` / `charge_vector` 的物理字节 | 是（commit→allocated→peak） | `memory.cpp:539-595`、`storage.cpp:302-337` |
| Arena 未 claim 容量 | held（不是 allocated） | `memory.cpp:391-407` |
| 科学侧 `std::vector`/`std::string`/CSR/QR/FFT/MPFR 工作集 | **否**：全仓库无 allocator hook（`grep operator new` 只有 runtime 自己） | `src/runtime/memory.cpp:257-266,568`；各 numerics/geometry `.cpp` |
| `BridgeResources::reserve`（科学模块声明的 workspace） | 只 `claimed`，永不 `commit`；`peak_allocated` 不含 | `science_bridge.cpp:180-205`、`memory.cpp:122-137` |
| `BridgeStorage::copy` 的拷贝 | 先分配后 claim；claim 失败也不留账 | `science_bridge.cpp:57-81` |
| `workflows::ingest_array` | **完全不记账**、无 Lease、persistent | `normalize_runtime.hpp:60-90`；实测 +4,195,500 B 分配、peak 仍 0 |
| 在途 I/O 的 staging | claim（≤8 MiB），但 payload 的 `std::vector`（encode 整块、压缩输出）不计 | `storage.cpp:759-782`、`701-721`、`743-751` |
| spill 活存磁盘 | 无账本项；只有累计 `spill_written_bytes` | `storage.cpp:1646-1647`、`514-533` |
| `BlockStore` 元数据/`block_index_` | 不计、不删 | `storage.cpp:1886-1927` |
| 产品发布时 `array_bytes`+`std::string` 的整数组拷贝 | 不计 | `product.cpp:85-96`、`767-780` |
| workflow 的 `contributions` 累积（每像素一行） | 不计；RSS 实测 31→44 MiB（16384 行） | `normalize.cpp` `contributions.push_back`；§6.3 |
| 控制节点 `Task`/continuation/取消节点 | 不计（`metadata_bytes` 字段存在但从未累加） | `internal.hpp:104`、`pool.cpp` 无 metadata 记账 |

`ScopeSnapshot.peak_allocated` 是**子树聚合**（每个祖先都在 commit 时更新），所以不会出现“子 scope 高水位求和冒充峰值”的错误；但 `Usage.peak_payload_bytes` 取自 `peak_allocated.host_bytes`（`host.cpp:185`），而真正的大头在 §5.1 的“否”行，因此**报告的峰值系统性偏低**，不能作为 M5 的“内存封顶证明”。

### 5.2 低 H 是否改科学量

**低 H 在生产路径上未生效**，因此“是否改科学量”无实验可做——这正是 P0-3。实测：X01 config `resources.bytes=1 MiB, spill_bytes=0, threads=1`，而每次 `compute_overlap` 的 plan 是 `r_bytes=2,100,416 + f_bytes=3,211,264 ≈ 5.3 MiB`，1024 次 invoke 全部成功并发布，进程 `maxrss=31,264 kB`。若 H 真被遵守，第一次 admit 就应 `resource_exhausted`（5.3 MiB > 1 MiB），或至少 spill 配额 0 应拒绝 spill。

### 5.3 两套账本/两个池

CLI 的 `CpuResources` 有独立 `bytes_in_use_/threads_in_use_/work_used_` 和 `workers_` 池（`cpu_resources.cpp:35-73,75-133`），runtime 的 `Services` 有独立 scope 树/ledger/池（`memory.cpp`、`pool.cpp`）。科学任务实际只在 `Services` 池跑，`CpuResources::reserve/consume_work` 在生产路径上没有调用者（grep 无科学模块调用 `resources.reserve`），所以 CLI 的用户额度既没约束 runtime，也没有被任何模块消费。

## 6. 实际运行证据

### 6.1 X01 单帧 normalize（真实 CLI，block=1）

配置：`validation/x01/input/cli/normalize.config.json` 只保留 `x01-00`（128×128），H=32 MiB，threads=1，输出到 `/tmp/rt-audit/x01d/products`。

```
$ /usr/bin/time ... build/science/acsd normalize --json normalize.config.json
exit=0
wall=109.25s  maxrss=44288kB
trace: compute_overlap = 16384 次；其他 step 共 14 次
stdout 最后一条:
{"assessment":null,"block":null,"code":"ok","completed":1,"event":"frame_finished",...,
 "message":"the frame was published",...}
```

即**一帧 128×128 = 16384 次 `geometry.overlap` Host invoke**，耗时 109.25 s，RSS 44 MiB（配置 H=32 MiB）。

### 6.2 同一帧、H=1 MiB（证明 H 未接线）

```
$ env ACSD_DRIZZLE_BLOCK=4 build/science/acsd normalize --json normalize.config.json   # resources.bytes=1048576
exit=0  wall=16.02s  maxrss=31264kB
trace: compute_overlap = 1024 次，每次 r_bytes=2100416, f_bytes=3211264
stdout: "the frame was published"
```

5.3 MiB 的 R+F 在 1 MiB 的 H 下成功 → 用户 H 没有进入运行时账本（P0-3）。

### 6.3 每像素 invoke 的代价（同一二进制 `cee8ce9a` 上的三点计时）

> 说明：`ACSD_DRIZZLE_BLOCK=2/4` 是**仅计时用**探针；它把 source row 从像素改成块，**会改变估计量**，因此产物不用于任何科学结论。审查期间其他成员已在工作树加入 `block_size>1 → unsupported` 的守卫（未提交），与本节结论一致：不能用 block>1 做修复。三点都只改变 invoke 数，总探测面积不变。

| block | invoke 数 | wall |
|---|---|---|
| 1 | 16384 | 109.25 s |
| 2 | 4096 | 34.26 s |
| 4 | 1024 | 16.02 s |

线性拟合 `T = c·N_invoke + A`：

```
c = (109.25 - 16.02) / (16384 - 1024) = 6.118 ms / invoke
A = 109.25 - 16384*0.006118 = 9.76 s
check block2: 9.76 + 4096*0.006118 = 34.8 s   (实测 34.26)
check block4: 9.76 + 1024*0.006118 = 16.0 s   (实测 16.02)
```

残差（面积相关工作量）在 9.2–9.8 s 内基本恒定，说明“每次 invoke 的固定代价”≈6.1 ms 与面积无关。再用与真实 plan 相同 R/F 的 toy process 测 Host 生命周期（§6.4-S5b）：**0.11 ms/invoke**。因此 ≥98% 的每次调用代价在 `compute_overlap` 的每次调用固定 setup（chart/精确裁剪/identity/对象构造），不是 Host 派发。

### 6.4 独立 C++ 实验（`/tmp/rt-audit/exp/exp.cpp`，真实输出）

S4：workflow ingest 的账本漏算
```
== S4 workflow ingest accounting ==
open_workflow_runtime: ok
command-before held=0 claimed=0 allocated=0 peak=0 work=0 read=0 written=0 spill_written=0
LocalStorage::copy(4MiB): ok
ingest_array: ok; allocations during ingest=4195500 bytes
command-after-ingest held=0 claimed=0 allocated=0 peak=0 work=0 read=0 written=0 spill_written=0
```

S1：begin/append/seal/read、read_reserved 缺失、spill、写失败
```
== S1 storage ==
reserve 8MiB arena: ok
begin(memory,4MiB,1MiB-chunks): ok
append(4MiB in 1MiB-chunks): ok elements=17039360
seal: ok blocks=4
after-seal held=12582912 claimed=65536 allocated=8388608 peak=8388608 work=1120 read=0 written=4194304 spill_written=0
read after seal: ok sampled_equal=1
read 4MiB with 1MiB arena: resource_exhausted (no read_reserved in the public Storage interface)
spill append/seal: ok/ok blocks=4
after-spill-seal held=22020096 claimed=131072 allocated=12582912 peak=12582912 work=3265 read=0 written=8388608 spill_written=4194304
failure-begin: ok
forcing write error by chmod 0555 on /tmp/rt-audit/exp/spool/writer-4
append with unwritable spool: io_error
seal after failed append: io_error (must not be ok)
abort after failed append: ok
```
spill 后 4.1 MB 私有 spool 残留在 `/tmp/rt-audit/exp/spool/writer-3`（sealed 的正常保留）与 `writer-4`（**写失败后本应清理却因只读目录未被删除**，`cleanup_writer` 忽略 `remove_all` 错误）：
```
dr-xr-x--- writer-3  4 files 4.1M
drwxr-xr-x writer-4  4 files 4.1M
```
关于 P1-16（`Unit` 未初始化）：上一版实验在完全相同的 unit 字段下得到
```
append issue: block unit does not match the writer
dbg: block.index=1 writer.index=sha256:06bb... block.hash=sha256:06bb... unit_mul=0/0 lease=1 begin=0 count=524288
```
显式 `spec.layout.unit = Unit{}` 后才通过——证明默认构造的 `Unit` 成员是 indeterminate，`unit_equal` 读到了未初始化值。

S2：pipe 有界/EOF-gap
```
send item=0 -> state=success
send item=1 -> state=waiting_dependencies
send item=2 -> state=waiting_dependencies
send item=3 -> state=failed
receive first=0
close(eof) with parked senders: ok; second receive code=ok end=0 kind=-1
parked send item1 state=success item2 state=cancelled
```
说明：`receive first=0` 里的 `promote()` 已把 item=1 移入 ready，所以 close 后的第二次 receive 正常拿到 item=1（`end=0` 是 `holds_alternative<PipeEnd>` 为假，不是缺陷）；item=2 在 close 时被取消。**EOF-gap → failed/missing_dependency 的改判是静态代码**（`pipe.cpp:542-556`），本实验没有再取第三次 receive 去观察该终态。

S3：broadcast 取消永久挂起（P0-4）
```
== S3 broadcast + cancel ==
make_pipe two edges: ok/ok
broadcast key0 (both have credit): state=success
broadcast key1 (both full -> parked): state=waiting_dependencies terminal=0
after request_cancel + 400ms: state=waiting_dependencies terminal=0
after both pipes close: ok/ok parked state=failed
```
取消后 400 ms 仍非终态；只有“另一条无关的 pipe close”才把它唤醒为 failed。若没有这次 close，等待该 Task 的 `wait_external` 永久阻塞。

S5：Host 生命周期（toy process，真实 registry）
```
invoke x2000 ok=2000 total=270.8 ms per-invoke=135.4 us
```
S5b：与真实 `compute_overlap` 相同 R/F（2,100,416 + 3,211,264）的 toy process，run 立即返回
```
invoke x1000 ok=1000 total=110.0 ms per-invoke=110 us
bench2-command held=0 claimed=0 allocated=0 peak=0 work=0 ... (R/F 只是 reserve，不入 peak)
```

### 6.5 产品发布与重开（真实 CLI）

```
$ acsd normalize ...   # x01-00
manifest: {"profile":"full","complete":true,
           "capabilities":[...,"omitted:the covariance model is not reopenable:
            the action closure is not reopenable: the array acsd.Array:27cd... is not part of the product"]}
SignalEstimate: {"signal":{...},"support":{...},"noise":null,"error":null}
发布目录同时含 manifest.json 与 manifest.staging.json(complete=false)
```
用新进程从该产品 export（`expected_hash` = 用 `workflows::canonical_id(manifest.json)` 独立算出）：
```
exit=0
{"event":"product_published","message":"the product was published at .../export-from-normalize"}
{"event":"run_finished","exit_code":0,"summary":{"published":1,...}}
新产品 manifest: {"profile":"full","complete":true,"capabilities":[...]}   # 已无任何 omitted: 记录
新 SignalEstimate: noise=null
```
即：`full` 产品丢了 covariance 闭包仍标 complete；重开不恢复 noise；再生时遗漏记录被抹掉。

### 6.6 不能证明/未执行

- 未跑 ASan/UBSan/TSan、未做故障注入（SIGKILL、只读/满盘文件系统镜像、掉电），因此“无数据竞争/无 UAF”没有形式结论；本文所有并发结论都是代码路径推理 + 一次取消/写失败实验。
- 第三方库行为未证明：CFITSIO/WCSLIB/MPFR/GMP/BLAS/Qt 的线程安全、`fenv` 与编译器优化的交互、locale、allocator 行为。仓库未链接 WCSLIB/ERFA（`build/science` 的 `acsd` 只链 runtime/numerics/model/zstd/cfitsio），MPFR 也未在本次实验中触发；这些只能条件化，不能声明已认证。
- 6.1 ms/invoke 中“compute_overlap 内部 vs bridge”的精确切分未插桩；结论由“Host 生命周期实测 0.11 ms + 面积无关残差恒定”排除得到，属有界推断。
- 未验证真实大帧（4096×4096）下的 `contributions` 内存上界；按每像素约一行、`OverlapContribution` 含 3 个 `SphericalAreaInterval`（每项含字符串 `Id` + `ExactInterval` 两个 `Dyadic` 字符串），4096² 像素的累积量级在 GB 级（数量级估算，非实测）。
- `build/science/acsd` 与当前工作树源码不完全一致（他人并发修改中）；本文 CLI 计时全部锁定二进制 hash `cee8ce9a`，代码行号锁定 HEAD `ca7de6d4d`。

## 7. X01 的 20 分钟级 overlap 开销：根因与最小修复方向

### 7.1 事实

- 8 帧 × 128×128 的 X01 记录约 41 分钟（`docs/audit/review-m6.md`）；单帧实测 109 s。
- 单帧 16384 次 `geometry.overlap`/`compute_overlap` Host invoke（trace 实测），即 **1 像素 = 1 次 invoke**：`block_size=1` 默认（HEAD `normalize.cpp:936-938`），循环 959-990。
- 每次 invoke 的固定代价 6.12 ms（三点拟合，残差 <0.6 s），其中 Host 生命周期 0.11 ms。
- 每次 invoke 的 plan 声明 `R=2,100,416 / F=3,211,264`，且 block=1/2/4 三个配置下**完全相同**——plan 不随 footprint 面积变化。
- workflow 在 `contributions` 中累积每次 invoke 的结果（16384 行 → RSS 31→44 MiB）。

### 7.2 根因（分层，按贡献排序）

1. **工作单元粒度错误（主因）**：`normalize` 把“一个探测器像素”当作一次 process invoke。`compute_overlap` 的每次调用有 ~6 ms 固定 setup（WCS chart、MPFR/精确裁剪上下文、`GeometryRecipe`/`Overlaps` 对象与 identity 构造），与面积几乎无关。每帧 16384 次调用把这份固定代价放大 16384 倍 → ~100 s/帧。
2. **plan 不随工作单元缩放**：`plan_compute_overlap` 只用 `footprint_vertices`（4 个顶点）和 `budget.max_work`（clamp 4096）算 R/F（`src/geometry/overlap_process.cpp:675-696`），与一次调用要处理的源行数/叶数无关；因此“每像素 5.3 MiB”既过大（对 1 像素）又对更大 cohort 不足（M5 的资源上界证明不成立）。
3. **结果全量累积**：workflow 把每次 invoke 的 contributions `push_back` 到无界 `std::vector`，再一次性 `build_drizzle`；内存 O(像素数×叶数) 且不计账。真实 4096² 帧不可行。
4. **Host/派发不是瓶颈**：0.11 ms/invoke（含 child scope、plan、reserve/claim、settle 链、control-lane 往返）；即使把 Host 优化到 0，也只能省 <2%。
5. **“全量 ingest”是内存问题，不是这 100 s 的主因**：`Storage` 公共接口没有 `read_reserved`，workflow 只能整数组 `copy` 进 BlockStore（P1-14：4.2 MB 无账本、无 Lease）；它让大数据帧的 memory 模型失真，但单帧 128×128 的 ingest 量很小。

### 7.3 可证明保持科学结果的最小修复方向（不改代码，只给方向）

**目标**：把“每像素一次 invoke”改成“每次 invoke 处理一个有界 source-row cohort”，同时保持 drizzle 的 **source row 仍然是像素**、`a_jp` 与归约顺序逐位不变。

1. **在 process adapter 内做有界 cohort 循环**：`plan` 显式声明 cohort 的 source-row 数上界（按 R/F 预算算，而不是固定 1 像素）；`run` 在一次 `submit_cpu` 的 task 内循环处理 cohort 内的像素（每个像素仍调用同一个 `compute_overlap` 实现、同样的 WCS/footprint/预算），逐像素 `consume_work`，并在每 N 个像素（≤4096 work 或 ≤100 ms 的现有取消粒度）检查取消。
2. **workflow 只负责切 cohort 与合并**：把现在的 `for (block...) invoke(...)` 换成对 cohort 的有限循环（cohort 数由 plan 给出），每次 invoke 返回该 cohort 的 contributions；合并顺序仍按像素 flatten 序，`build_drizzle` 的输入（`Overlaps.contributions` + `observation`）与 R 行定义不变。
3. **禁止用 `block_size>1` 降调用数**：那会把 source row 从像素改成块，`x_p` 与 `a_jp` 的估计量都变（工作树已由他人加入 `unsupported` 守卫，方向正确）。同理不能用“合并像素后重新归一”或“降 pixfrac/order”换速度。
4. **可证明性**：对同一帧比较新旧实现的
   - 每个 `OverlapContribution`（source/leaf/native_area/drop_area/overlap_area、exact 端点）逐项相等；
   - `build_drizzle` 输出的 `Overlaps`/`DrizzleResult` 的 signal identity、`normalizer_sr`、support rows 相等；
   - product manifest 的 `SignalEstimate.signal` content hash 相等（`Usage`/耗时/plan 不在科学 hash 内）。
   由于只有“同一次调用的像素分组”变化、像素级数据与归约序不变，这是可判定的等值比较，不需要容差。
5. **预期收益**：把 `N_invoke` 从像素数降到 cohort 数（例如每 cohort 256–1024 像素），overlap 期约 100 s/帧 → 接近 9–10 s/帧的面积工作下限；对真实大帧同时把 `contributions` 的峰值从 O(像素) 降到 O(像素)（不变，但可流式交给 drizzle 的分块接口，另行设计）——至少不会因 invoke 数线性放大。
6. **配套（否则收益会被抵消）**：把 `plan` 的 R/F 改成 cohort 的真实上界（当前按 1 像素声明，cohort 化后会严重不足）；把 workflow 的硬编码 1 GiB/2 token 换成 `context.resources` 的实际额度（P0-3）。

## 8. 可复现故障清单（按优先级）

| ID | 现象 | 最小重现 | 根因位置 |
|---|---|---|---|
| F1 | 取消后 broadcast 等待永久不结束；组无上限增长 | `/tmp/rt-audit/exp/exp` 的 S3；或两 edge pipe 各自 `max_items=1`、先填 1 项，再 `broadcast(key=1)` 后 `request_cancel`，观察 `Task::state()` 400 ms 仍非终态 | `pipe.cpp:52-55,280-303,630-642` |
| F2 | config H=1 MiB 仍以 5.3 MiB plan 发布 | 6.2 节命令 | `normalize.cpp:349-354,381-387`、`normalize_runtime.hpp:303-312` |
| F3 | `profile=full, complete=true` 的产品缺 covariance，重开后 noise=null；再发布后遗漏记录消失 | 6.5 节 | `product.cpp:742-754,909-910`；`product_io.cpp:729,734-741` |
| F4 | `append_async` 大块 append 返回错误累计数 | `/tmp/rt-audit/exp/exp` 的 S1（buffer_bytes=64 KiB，append 524288 个 f64 → 17039360） | `storage.cpp:1467-1478` |
| F5 | 写失败 + 只读 spool 目录 → 4.1 MB 私有文件残留、无 orphan 记录 | S1 的 chmod 段 + `ls /tmp/rt-audit/exp/spool` | `storage.cpp:467-480` |
| F6 | 1 MiB arena 读 4 MiB 数组被拒（无 `read_reserved`） | S1 | `model.hpp:99-104`、`storage.cpp:1032-1059` |
| F7 | workflow ingest 4.2 MB 而账本峰值 0 | S4 | `normalize_runtime.hpp:60-90` |
| F8 | 默认构造 `Unit` 导致 `append` 被拒（未初始化读） | S1 的早期输出 | `model.hpp:32-34`、`storage.cpp:72-76` |
| F9 | 未知 action recipe 被当 identity 接受 | 构造含 `"kind":"future_kind"` 的 LinearAction 对象过 `decode_action_recipe` | `product_io.cpp:805-835` |
| F10 | 发布目录残留 `manifest.staging.json` | 任意 publish | `product.cpp:912-916`（未 unlink） |
| F11 | 无 fsync 的发布（静态事实） | `grep -rn fsync src` 无命中；`product.cpp:68-84,944` | P1-5 |

## 9. 建议修复顺序

1. **P0-3/资源接线（先做，否则所有低 H/取消/拒绝实验都无效）**：workflow 用 `context.resources` 的 Limits 创建 `Services`/command；删除三处硬编码；删除第二套 `CpuResources` 池或让它成为 `Services` 的唯一 gate（二选一，不能两套账本）。
2. **P0-4/P1-12/13 有界与取消**：broadcast 停放组纳入 command scope（带 cancel watcher）、加组数上限；`command->waiting` 加 `metadata_bytes` 配额；`close_command` 的 drain 计入 I/O 节点。
3. **P0-1/P0-2/P1-9 产品闭包与 complete**：`complete` 永不升级；closure 失败 → 该 profile 不发布或降级为具名 `signal_only` 且 `omitted` 进入产品能力并在再生时传播；重开必须恢复 `noise`（或明确 `noise=unknown` 并标 capability）；未知 recipe/version → `unsupported`。
4. **P1-5/7/8 发布原子性与输入安全**：fdatasync 文件+目录、临时文件清理、路径根约束、decompress 上界准入。
5. **P1-1/2/3/4/6 Storage 正确性**：append 返回值、checked 乘法、spill 配额/活存账、orphan 记录、canonical 块与 descriptor id。
6. **P0-6/P1-14/15 真实计费**：科学侧分配必须经 `Buffer`/`Arena`（或受控 allocator）并 commit；ingest 计费/加 Lease/可逐出；`block_index_` 随 Array 释放。
7. **§7 X01 cohort 化**（按 7.3 的等值证明门槛），同步修正 plan R/F。
8. **观测（R4）**：CLI `run_summary` 增加 H/peak/spill/in-flight 与真实线程数，事件流写失败使退出码失败。

## 10. 未决问题与复核条件

- 本报告的部分 `file:line` 已被审查期间的并发未提交修改触及（`normalize.cpp`、`mosaic.cpp`、`export_projection.cpp`）；修复落地后须由另一名未参与实现的审查者按同一 §8 清单复跑，尤其 F1/F2/F3/F4/F5 必须在新二进制上重新观察。
- `compute_overlap` 每次调用 6 ms 的内部构成仍需插桩（按调用阶段计时）才能把“chart/精确裁剪/identity 构造”分别归因；在此之前 7.3 的 cohort 化是可执行的，但其收益下界应以新实测为准。
- Dyadic typed read、ST-OVERFLOW、ST-REOPEN、PIPE-CHAIN/POOL01/ASYNC01 仍未有本轮的通过证据；`docs/audit/IMPLEMENTATION-STATUS.md` 的 E-02/E-05 与本报告重叠，本文只提供可复现证据，不改变其状态标签。
- X01 的科学真值比较（`validation/x01/comparison.json` 的 `pending_implementation`）不由本报告覆盖：本文只证明“发布链可跑”与“资源/闭包不满足合同”，不证明 X01 的估计量正确。
