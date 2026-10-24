# M0—M4 独立复核报告（只读）

> 复核者：reviewer-m0m4（未参与 M0—M4 设计/实现；只读代码与证据，不改 `src/`、`include/`、`apps/`、`validation/`，不 commit/push）。
> 交付物：本文件。复现脚本全部在 `/tmp`（`/tmp/repro_*`、`/tmp/audit/*`），未写入 `validation/`。

## 0. 锚点、方法与环境

| 项 | 值 |
|---|---|
| 指定复核提交 | `cea5e3650`（M0 头）、`0a5dbfff3`（M1—M3/registry）、`33804ef6a`（M4）、`073d91285`（validation 包）、`d404c99f7`（audit matrix） |
| 复核开始 HEAD | `d404c99f7`；复核期间分支又前进到 `f9c6f9b19`、`6fe0c8aac`（00:17，修了 work 预算拒绝 + 刷新 validation 快照） |
| 本报告行号锚点 | `6fe0c8aac` 的 `git show HEAD:<path>`（`task.hpp`/`pipe.cpp`/`storage.cpp`/`memory.cpp`/`calibration_*.cpp` 自 `d404c99f7` 起未改；`pool.cpp` 仅多 3 行 work 检查；`builtin_processes.cpp` 在 `f9c6f9b19` 有生成装配重构） |
| 工作树状态（写作时 00:37） | 52 项未提交改动：`include/acsd/runtime/{task,pipe}.hpp`、`src/runtime/{pipe,pool,memory,internal}.*`、`src/observation/calibration_signal.cpp` 等正在被并行实现修改。**本报告不对这些未提交改动作任何背书**；若工作树已修掉某条，请以本报告锚点提交的复现为准。 |
| 构建/运行 | `/tmp/acsd-build`（`ACSD_DEV_MODE=OFF`，RelWithDebInfo）+ `/tmp/acsd-build-dev`（ON）。公开头探针用 `g++ -std=c++20 -O2 -I include` 链接 `libruntime/libmodel/libnumerics/libstorage/libresources/libcodec`；M4 探针额外链接 science 库。 |
| 规范正本 | `SYSTEM.md`、`FOUNDATION_API.md`、`SCHEDULER.md`、`DEVELOPMENT.md`、`COMPUTE_MODULES.md`、`CALIBRATION_WALKTHROUGH.md`、`VERIFICATION.md`、`processes/observation.calibrate.md`、`processes/README.md`、`OBJECTS.md`、`EXECUTION.md`、`API.md`。 |

**方法**：先冻结锚点（`git show`），再按“规范条款 → 公共头 → 实现 → 最小构型”逐项追；对每个疑点写 `/tmp` 探针用公开头复现；对证据做“提交内 observed/comparison/README/manifest 四方对照 + 亲自重跑”。不接受“测试通过”作为唯一证据，也不接受“编译/链接成功”作为语义证据。下文标注“**已复现**”的条目是我在 `/tmp` 亲自运行得到的；未标注的条目是代码/证据审读推出的可证伪构型（构型已给全，可直接按 §9 执行）。

---

## 1. 结论

| 里程碑 | 判定 | 一句话 |
|---|---|---|
| M0 公共合同与登记 | **ACCEPT（带 P2 caveat）** | 54 个规范类型/58 个可调用项全部落在公共头；freeze 负例实现完整且可由 `acsd_registry_negative`（107 checks）+ `reg01`（28 checks）复现；生成物 `--check` 通过、manifest 22/57 覆盖 PASS。剩 P2：模块 descriptor 与生成 descriptor 两套来源且依赖字段不一致；coverage 不比对依赖；空 operation 的 process 与共用文档可通过 freeze；`when_all`/`then` 的语义细节。 |
| M1 账本/所有权 | **ACCEPT-WITH-CAVEATS** | reserve/claim/commit/release/close 的祖先原子检查、失败零副作用、committed 先物理释放再减账、close 后存活 Lease 仍计费均成立（S-LIFE/A01/life01 20/20 可复现）。但 `work_upper=0` 可绕过 `max_work`（P1，已复现）。 |
| M2 Task/pool/pipe | **REJECT** | `when_all` 对非法/被移动 Task 产生 UB 且可返回伪造成功值（P0，已复现）；ordered pipe 无法跨 cohort/item 推进，永久 stall（P1，已复现）；无 2P ready window（P1）；broadcast/close/metadata/control-ceiling 等 P2。唯一池、加权轮转、取消排空本身成立。 |
| M3 异步 Storage/诊断 | **REJECT** | seal 与 abort 竞争可让 abort 后 seal 成功并交付 `complete=1` 但不可读的 Array，并在 spill 路径出现 SIGSEGV/堆损坏（P0，已复现）；abort 不排空在途 seal 写（P1）；未 sealed writer 析构不清理、spool 泄漏（P1）；`buffer_bytes` 不计费（P1）；已封存数组被 BlockStore 永久钉住、容量永不归还（P1，已复现）。读路径、短读、hash 校验、单在途 append、seal 元素数、abort 幂等基本正确。 |
| M4 observation.calibrate | **REJECT（作为“完成/全链验收”声明）** | kernel 唯一实现、Host 全链可跑通、`cal_host` 25/25 可复现且逐位一致；但同步入口与 Host 对同一输入判定不同（P1，已复现）；plan 的 work/R/F 与实际分配不符（P1）；C-CAL-WRITE/C-CAL-CLEAN 未跑且 C-CAL-WRITE 实际是 P0-2 的破坏路径（P1）；plan 声明 pipe edges 但组合器不用 pipe/credit（P2）；C-CAL-ROOT 证据是构造性必真（P2）。 |

**总裁决：REJECT**（针对“M0—M4 完成”的整体声明）。分项：M0 通过、M1 有条件通过、M2/M3/M4 拒绝。P0 必须清掉才可重新提交复核；P1 需修复或给出“能力边界 + 文档降级”。

---

## 2. M0 公共合同与登记

### 已核实的正确部分

1. **类型/签名覆盖**：脚本从 `FOUNDATION_API.md` 的 6 个 cpp 代码块抽出 54 个 `struct/class/enum class/using` 与 58 个可调用名，逐个在 `include/acsd/**` 找到同名声明，**无缺失**；`types/memory/task/pipe/storage/process/diagnostics` 与规范字段逐项对照，未发现缺字段或改语义的签名（`Key` 额外的 `<`/`==`、`PipeSpec` 在 types.hpp、`InvocationPlan` 字段顺序等均与规范一致）。
2. **唯一聚合入口**：`include/acsd/contracts.hpp` 仅 18 行 include；`model.hpp` 单拥有科学 DTO；`tools/generate_contract_schema.py` 从同一来源生成。
3. **freeze 负例**（`src/runtime/registry.cpp`）：重复 process（296-311）、重复 operation（313-329）、空 process/operation id、TypeId 重复/一 C++ 类型绑两 TypeId（331-376）、codec 缺 encode/decode/schema/cookie（335-354）、operation 缺 validate/bounds/plan/run/finalize 任一 hook（508-521）、input/output TypeId 未登记或 cookie 不符（528-551）、文档不存在（390-399）、未知模块依赖（423-438）、依赖环（460-506）、probe 空 id/空 kind/revision 0/重复/跨 process owner 冲突（400-422, 443-458）、重复 freeze（563-567）。实现与规范条款一一对应。
4. **生成物真实性**：`python3 tools/generate_typed_bindings.py --check` → `typed bindings are up to date`（exit 0）；`python3 tools/check_design.py` → PASS；用真实装配导出的 manifest 跑 `tools/check_process_coverage.py --manifest /tmp/manifest.json` → `PASS: 22 processes, 57 operations`。`acsd_typed_smoke`（1255 checks，22/57/102 TypeId/41 provisional bounds）、`acsd_registry_negative`（107 checks）、`acsd_host_lifecycle`（66 checks）在复核时重跑均 exit 0。

### 发现

**M0-1（P2）模块手写 descriptor 与生成 descriptor 是两套来源，依赖字段不一致。**
- 位置：`src/observation/calibration_process.cpp:260-267`（登记用 descriptor：`{"model","numerics","operators","uncertainty"}`）vs `src/runtime/generated/builtin_processes.cpp:43-45`（生成表：`{"model","operators","uncertainty","numerics","runtime"}`）；规范要求见 `CALIBRATION_WALKTHROUGH.md:107-112`（示例含 `"runtime"`）与 `SYSTEM.md:88,102`（“不手写三套白名单”）。
- 期望/实际：实际冻结出的 manifest（`acsd-runtime-manifest`）里 `observation.calibrate.dependencies = [model,numerics,operators,uncertainty]`，**缺 `runtime`**，尽管 `calibration_process.cpp:25`、`calibration_signal.cpp:24` 直接 include `runtime/internal.hpp`。
- 为什么没被检查拦下：`tools/check_process_coverage.py:300-310` 只校验依赖名合法，不比对 `SYSTEM.md` 的依赖表或生成表。
- 最小修订：模块登记改用 `generated::descriptor_for(process_id)`（f9c6f9b19 已提供）或在 coverage 中比对生成 descriptor 的 dependencies/probes；两者取其一，禁止并存。

**M0-2（P2）freeze 允许“零 operation 的 process”进入只读 registry。**
- 位置：`src/runtime/registry.cpp:588-601`（只校验已登记 operation 的 hook/type；没有“每个 process 至少一个 operation”的检查）。
- 构型/实际：`begin({"test.empty",...})` 后直接 `freeze()` → ok，manifest 中出现该 process 且 `operations: []`。规范侧 `SYSTEM.md:102` 要求“缺 binding 直接失败”，可解释为 process 级也必须至少一个 binding。
- 最小修订：freeze 时对 `processes` 中每个 id 检查 `operations` 至少一条（或由生成覆盖检查拒绝空 process）。

**M0-3（P2）`then` 只在 `code==ok` 时调用 F，不要求 value 存在；`Task::state()` 对非法 Task 返回 `success`。**
- 位置：`include/acsd/runtime/task.hpp:182-190,193`（仅判 code；`take_value<T>` 对无 value 的节点返回默认 `T{}`）；`:94-96`（`node_ ? state : success`）。
- 规范：`FOUNDATION_API.md:116`“仅 code=ok 且 value 存在才调用 F”；`Task()` 是非法句柄，不应当被读成 `success`。
- 可达性：`ready`/`submit_cpu` 会把“ok 无 value”降为 internal，正常路径不易触发；但它与 P0-1 的 `when_all` 同源（默认构造 Task 被当作成功），所以建议一并修。
- 最小修订：`then` 增加 value 检查；非法 Task 用显式 invalid 状态（或 `state()` 返回 `failed` 并带 invalid_request），不要用 success 冒充。

**M0-4（P2）`when_all` 的失败合并没有实现固定的 Code/Key 优先级。**
- 位置：`task.hpp:286-292`：按输入下标顺序取第一个非 ok，不按“协议/模型/完整性/internal/io_error > 取消 > 非 ok 科学/预算 > ok；同级取最小 Key”合并（`FOUNDATION_API.md:124`），且丢弃 Usage/dependencies。
- 构型：两个子任务，下标 0 为 `cancelled`、下标 1 为 `io_error`；实际聚合 code=cancelled，期望 io_error 优先。
- 最小修订：按 Code 类优先级 + Key 排序选择主因，其余按 (Key, ordinal) 并入 issues。

**M0-5（P2）文档校验只查“文件存在”，不查唯一性/归属；零 operation 的 process 与共用文档都能 freeze。**
- 位置：`registry.cpp:61-87`（`document_exists` 只做 `is_regular_file`）、`:588-601`（无“每 process ≥1 operation”与“每文档唯一 owner”检查）。
- 构型（均已复现）：(a) `begin({"test.empty",...})` 不 add 任何 operation → `freeze()=ok`，manifest 里 `operations: []`；(b) 两个 process 都声明 `docs/engineering/rebuild/processes/observation.calibrate.md` → `freeze()=ok`，两个都进 manifest；甚至可以把任意已存在的仓库文件（如 `README.md`）作为某 process 的 document。
- 期望：`SYSTEM.md:98`“唯一 process 文档路径”、`:102`“缺 binding…直接失败”。
- 最小修订：freeze 记录 `document -> process_id` 反向映射并拒绝重复/越界路径；要求每个 process 至少一个 operation。

---

## 3. M1 账本、Arena、Claim/Lease

### 已核实的正确部分

- `reserve`：先锁 root→scope 全链、检查 `pinned<=host` 与所有祖先 `held+v<=ceiling`，再整批 `held+=v`；失败不改账（`src/runtime/memory.cpp:99-120`）——与 `SCHEDULER.md:22-31` 一致。
- `claim/commit/release`：claim 只减 arena `available`、增祖先 `claimed`、不增 peak（`memory.cpp:423-451`）；allocate 成功后在同一票据 `committed=true` 并 `ledger_commit`（claimed→allocated、更新 peak；`memory.cpp:560-583`）；`Allocation::~Allocation` 先 `destroy(memory)` 再 `claim.reset()`（`memory.cpp:226-255`），满足“先物理释放再减账”。
- close：原子标 closed，把未 claim 的 `available` 退祖先 `held`（`memory.cpp:453-467`）；close 后存活 committed 票据在最后 Lease 死亡时 release 并 `ledger_return_held`（`memory.cpp:233-255`）——S-LIFE 语义成立（`life01` 20/20 ASan 可复现）。
- peak：只在成功 commit 时按 `allocated` 更新，claim/失败分配不更新；release 不减历史 peak（`memory.cpp:128-149`）。
- 失败零副作用与不重复扣父预算：`ledger_admit_work`/`ledger_consume_work`/`ledger_release_reserved_work`（`memory.cpp:157-204`）在 S-WORK 正常路径成立（`work01` 15/15→HEAD 16/16 可复现）。

### 发现

**M1-1（P1）`TaskSpec.work_upper == 0` 时 `consume_work` 不设上限，可越过父 `max_work`。**
- 位置：`src/runtime/memory.cpp:503-507`（只有 `work_upper != 0` 才检查 `own_work+units<=work_upper`）；`src/runtime/pool.cpp:287-300`（`work_upper==0` 不做准入预留）；`ledger_consume_work`（`memory.cpp:182-192`）对 `work_reserved` 已是 0 的祖先直接 `work += units`，不再检查 `max_work`。
- 构型：`PoolConfig.process.max_work=10`；单任务 `TaskSpec{work_upper=0}`，callable 内 `consume_work(task,11)`。
- 期望/实际：期望 `resource_exhausted`（`SCHEDULER.md:46`“已声明上限不足返回 resource_exhausted，不在运行中越过承诺”）；实际 `consume_code=ok`、`snapshot(ctx.scope).work=11 > max_work=10`。
- 复现：`/tmp/repro_work0`（源码 `/tmp/repro_work0.cpp`；`g++ -std=c++20 -O2 -I include /tmp/repro_work0.cpp -L/tmp/acsd-build -Wl,--start-group -lruntime -lmodel -lnumerics -lstorage -lresources -lcodec -Wl,--end-group -lpthread -lzstd -o /tmp/repro_work0 && /tmp/repro_work0`）→ `VIOLATION=yes`。
- 最小修订：把 `work_upper==0` 视为“本执行段不得消费 work”（或至少仍检查所有祖先 `work+units<=max_work`）；`consume_work` 的检查与 `ledger_consume_work` 在同一把 scope 锁下完成。

**M1-2（P2）claim 的 arena 状态与祖先 claimed 不是同一原子事务。**
- 位置：`memory.cpp:433-447`（先在 arena 锁内减 `available`，解锁后才 `ledger_claim_committed`）。并发 `snapshot` 可短暂看到 `held` 已含该容量而 `claimed` 未计入；`Arena::close` 同理由 `:453-467` 分两步。
- 影响：只影响并发快照的中间态，最终账正确（我用 close/claim 交错构型推演未得到漏记/双记），故 P2；但与 `SCHEDULER.md:22`“一次写入”不符。
- 最小修订：在进入 arena 锁前先锁祖先链（保持 root→scope 顺序），在锁内同时更新 available/claimed。

**M1-3（P2）`ScopeSnapshot.cpu_ns` 永远为 `nullopt`、`metadata_bytes` 永不累计。**
- 位置：`memory.cpp:531`（`out.cpu_ns = std::nullopt`）；`pool.cpp:271`（task 子 scope `metadata_bytes=0`）；`ScopeState::metadata_bytes` 只在 `host.cpp:209` 被读、从无递增。
- 影响：`SCHEDULER.md:16` 的 control metadata 额度形同虚设；usage 里没有 CPU 归因（规范允许“不可得 null”，但这里从实现上永远不可得）。
- 最小修订：在 dispatch 累计每段 CPU 时间；metadata 在 TaskNode/pipe item 分配时计入并受 `metadata_bytes` 限制。

---

## 4. M2 Task / 唯一 CPU 池 / Pipe

### 已核实的正确部分

- 唯一池/唯一账本：`Services::create` 用 `g_active_services` 拒绝第二个活跃池（`pool.cpp:337-345`），`cpu_threads` 被 clamp 到硬件核数（`:350-353`）；`acsd::rt` 内只有 `pool.cpp` 起线程；旧 `CpuResources` 池从 `rt` 不可达（`science_bridge.cpp:151-190`）。
- 加权轮转与 command ceiling：`multi01` 的整轮 1:2（30→120）与 steady 比率、池/命令并发上限在 HEAD 可复现；单 command 的 `cpu_running >= ceiling` 会跳过（`pool.cpp:942-946`）。
- 取消/关闭排空：`CancelState::request()` 在锁外跑 watcher（`internal.hpp:45-59`），pipe waiter（`pipe.cpp:280-303`）、容量 waiter（`pool.cpp:852-872` + `notify_capacity`）与 ready 节点（`run_node`/`close_command`/`shutdown`）均被唤醒；`shutdown` 等 `io_pending==0` 再 join I/O worker（`pool.cpp:1074-1084`）。仓库里旧的 `cancel_drain` P0 不再复现。
- 等待不占 CPU token：提交的 callable 返回 `Result<T>`，等待由 `then/settle` 的 continuation 承担；`take_runnable` 只在真正 dispatch 时计 `cpu_running`，parked 任务在 `waiting` 里不计 token。
- `Pipe` 正常语义：`send` 在 closed 后 `invalid_request`；close 线性化后先投递已接受 items 再给终态；queue 里的 payload 是 `shared_ptr<const T>`，`Block` 的 Lease 随队列存活；continuation 从不在 I/O worker 上跑（`task.cpp:58-88`）。
- control lane 有反饿死（每 8 次 science dispatch 至少 1 次 control，`pool.cpp:903-922`）。

### 发现

**M2-1（P0）`when_all` 对非法/被移动 `Task` 解引用未 engaged 的 `optional`：UB，并可返回“成功 + 伪造值”。**
- 位置：`include/acsd/runtime/task.hpp:268-276`（null 节点只 `fetch_sub`，非末位时不记 failure）；`:293-295`（最后成功路径对每个 `state->values[i]` 做 `std::move(*v)`）。对照 `settle_all` 在 `:325-332` 已正确处理 null，说明这是 `when_all` 的遗漏。
- 构型：`when_all(ctx, {pending_pipe_receive, Task<T>{}})`，`T=Delivery<uint64_t>`，随后 send 一个 item。
- 期望/实际：期望 `invalid_request`、无聚合 value（`FOUNDATION_API.md:118`）；实际有两种 UB 表现：(a) 旧构建在 `std::optional::_M_get()` 断言中止（exit 134）；(b) 当前构建返回 `code=0 size=2`，index 1 是默认构造的伪造值。
- 复现：`g++ -std=c++20 -O2 -I include /tmp/audit/when_all_probe.cpp -L/tmp/acsd-build -Wl,--start-group -lruntime -lmodel -lnumerics -lstorage -lresources -lcodec -Wl,--end-group -lpthread -lzstd -o /tmp/wa && /tmp/wa`。
- 最小修订：null 槽写入 `failures[i]={invalid_request,...}`；成功路径先检查所有 `values[i]` 均 engaged，否则返回 invalid/internal，绝不 `*v`。

**M2-2（P1）ordered pipe 的 frontier 只能在同一个 `(epoch,cohort,stage)` 内 `item+1`，跨 cohort/非零首 item 永久 stall。**
- 位置：`src/runtime/pipe.cpp:87-91`（`next_key` 只 `++item`）；`:386-396`（首个 send 把 `expected` 播种为 `Key{key.epoch,key.cohort,key.stage,0}`，admissible 要求 `key==expected`）；`:140-150`/`:174-180`（`promote` 同样只按 `expected` 放行）。
- 构型（已复现）：ordered `max_items=4`、窗口 256 B；先 `send({0,0,0,0})`（ok），再 `send({0,1,0,0})`（下一 cohort）→ 两个 Task 永远 `state=0`；或首个 `send({0,0,0,5})` → 永不投递。
- 期望：`FOUNDATION_API.md:231`“ordered 返回 next_required_key”、`SCHEDULER.md:62`“region 的 progress cohort 始终是尚未完成的最小 Key”；stream region 的 `cohorts>1` 是常态。
- 影响：当前 M4 的 `stream_map_ordered` 未用 pipe（见 M4-4），所以生产链未命中；但任何按规范用 ordered pipe 的多 cohort stream 都会死锁，S-ORDER/PIPE01 的“可行配置完成”不成立于该构型。
- 最小修订：`expected` 从 plan 的 stream 起始 key/cohort 上界生成，跨 cohort 与 stage 单调推进；不靠“首 send 猜 item=0”。

**M2-3（P1）没有 2P 已就绪窗口；ready/waiting/control 队列无界。**
- 位置：`src/runtime/pool.cpp:735`（`command->ready.push_back`）、`:717`/`:885`（`waiting.push_back`）、`:893`（retry 回 ready）；`internal.hpp:238-239,399` 是裸 `std::deque`。全文件没有 `2*cpu_threads`/`max_window` 的运行时比较（`PlanBounds.max_window` 只被模块写，运行时不用）。
- 构型：P=1，提交 100k 个 `work_upper=1` 任务 → 全部入 ready/waiting，无拒绝/阻塞。
- 期望：`SCHEDULER.md:11`“全进程最多 2P 个已就绪队列项；其余保存在已计费的有限计划窗口，不创建无界 future”。
- 最小修订：submit/admit 计数 ready+waiting，超过 2P 时拒绝或以 plan window 元数据计费停放；control 队列同样设上限。

**M2-4（P2）broadcast 不拒绝重复分支；组信用检查与插入不是同一原子区间（对同 pipe 的普通 send 可竞争）。**
- 位置：`src/runtime/pipe.cpp:559-595`：`broadcast_mutex()` 只序列化 broadcast 之间；每个分支的 `has_credit` 与 `push_back` 分两次加锁（`:561-580`），分支列表未去重。
- 构型（已复现）：`unordered`、`max_items=1`、`broadcast(k,v,{p,p})` → 两个 receive 都拿到数据（队列超过 max_items）。
- 期望：`FOUNDATION_API.md:233`“一次原子取得所有分支的 credit”“一个分支不消费时背压全部必需分支”。
- 最小修订：同一 `PipeCore*` 出现两次即 `invalid_request`；按 edge_id 排序后持全部分支锁完成 check+insert（或使用统一的 credit 事务）。

**M2-5（P2）`ScopeLimits.metadata_bytes` 不生效；未被观察的失败任务不进入 scope 日志。**
- 位置：`pool.cpp:271`（task 子 scope `metadata_bytes=0`）；`src/runtime/task.cpp:109`（`TaskNode::~TaskNode() = default`，无日志/无失败上报）。`FOUNDATION_API.md:121` 要求“未观察的失败进入 scope 日志”。
- 最小修订：metadata 在节点/pipe item 生成时累计并受配额；TaskNode 终态失败且无观察者时向父 scope 记 Issue。

**M2-6（P2）control lane 绕过 command 的 CPU ceiling。**
- 位置：`pool.cpp:910-921,970-978`（control 从 `control_.front()` 直接取，不计 `cpu_running`/credit）、`:1029-1032`（control 节点不增/不减 command 计数）；而 science 路径在 `:942` 检查 ceiling。
- 构型（subagent 实测）：command `cpu_tokens=1`、P=2、4 条 `then` 链各睡眠 60 ms → 峰值并发 2。
- 说明：`FOUNDATION_API.md:399` 允许 control continuation 在 stopping 时继续，但要求“只做有限控制组合……计算必须 submit_cpu”；这里没有对 control 的 CPU 计费，故 P2（重活应被要求 submit_cpu）。
- 最小修订：control 节点也计 command CPU token；或给 control 单独的、受 ceiling 约束的额度。

**M2-7（P2）read/write I/O slot 不按 `min(P,4)`/2 clamp。**
- 位置：`pool.cpp:354-355`（直接照抄 config）。`SCHEDULER.md:13` 要求 active read ≤ `min(P,4)`、write ≤2。
- 构型：`PoolConfig{cpu_threads=1, read_slots=8}` → 8 个读 worker。

**M2-8（P2）重复 close 的幂等比较只看 kind 和 issue 数量。**
- 位置：`pipe.cpp:492-495`。构型（已复现）：`close(failed,[io_error])` 后再 `close(failed,[internal])` → 两次都 ok；期望 `FOUNDATION_API.md:231`“冲突终态 invalid_request”。

**M2-9（P2）非 Block DTO 没有 `bounded_size` 登记入口，按 `sizeof(T)` 计费。**
- 位置：`include/acsd/runtime/pipe.hpp:38-48`（generic `pipe_payload_bytes` 返回 `sizeof(T)`，无登记 API）。构型（已复现）：含 8 MiB `vector` 的 DTO 通过 `max_queued_bytes=64` 的窗口。`FOUNDATION_API.md:227` 要求普通 DTO 登记 bounded_size、禁止无上界 payload。

---

## 5. M3 异步 Storage 与诊断

### 已核实的正确部分

- 读路径：短读/短尾是 `io_error` 而非零填充（`storage.cpp:1099-1106`）；解压长度校验（`:1136-1146`）；发布前逐块 raw SHA 校验（`:1150-1159`，故意破坏 raw/zstd 块 → `hash_mismatch`）；range/覆盖/gap 校验（`:975-1085`）；顺序是“预分配缓冲(CPU)→read slot→syscall→释放 slot→单独 `submit_cpu` 解码/hash”（`:985-987,1088-1110,1200-1210`），解码不在 I/O lane。
- append：一次一个在途（`storage.cpp:1389-1392`）；连续性 `range.begin==next_element`（`:1412-1415`）；dtype/itemsize/unit/index hash 校验；超 total 拒绝；seal 要求精确元素数且不静默修复（`:1505-1508`）；失败 append 后 writer failed、后续 append invalid_request、seal 返 io_error、abort 清理 spool。
- abort：顺序调用时幂等、seal 后是 no-op、已交付 Array 仍可读（`storage.cpp:1626-1632` 及其后的 cleanup 路径）。
- 规范身份：memory/spill 两种 placement 同值同 `content_hash`；块 id 不含路径/codec/stored_bytes（`hash_block` 250-255）；规范块 `2^20/itemsize`、dyadic 1024。
- 诊断：OFF 宏完全擦除参数且 OFF 不建 sink/不读 env（`diagnostics.hpp:90-97`、`pool.cpp:380-395`）；ON 先 selection 后构造 payload（`diagnostics.hpp:67-83`）；`try_*`/`allocate_span_id` noexcept；队列受 `max_items`+`memory_bytes` 限制；磁盘失败只增 `io_errors`；快照复制到私有 blob、返回后不持科学 Lease。

### 发现

**M3-1（P0）seal 与 abort 竞争：abort 后 seal 仍可成功并交付 `complete=1` 但不可读的 Array；spill 路径出现 SIGSEGV / 堆损坏。**
- 位置：`storage.cpp:1490-1509`（seal 只在同步 prologue 查 status）、`:1517-1530`（prepare 只查 `block_open`）、`:1549-1576`（finalize 只拒 `failed`，随后无条件 `status=sealed` 并注册 `state->blocks`）；`:1626-1649`（abort 只 defer `append_in_flight`，忽略在途 seal 写，直接 `cleanup_writer` 删除 spool）。
- 构型 A（memory，writer `shape{3000}`，append 全量后同时 `seal_async` + `abort_async`）：期望 abort 胜出或至少不交付半成品（`OBJECTS.md:142`“open→sealed 或 open→failed→aborted；任一未 sealed 析构等价 abort；seal 验证总元素数、块覆盖及 hash”）；实际 `abort.code=0 seal.code=0`、`complete=1`，随后读该 Array `invalid_model`（`block_ids` 在 0/1 间竞态）。
- 构型 B（spill，`shape{1MiB+100}` u8，buffer 1 MiB）：同一交错下多次运行出现 SIGSEGV(139)、`malloc(): unsorted double linked list corrupted`(134)、或 abort.ok+seal.io_error、或 seal.ok+read io_error/resource_exhausted——确证数据竞争/UAF。
- 复现：`/tmp/repro_seal_abort`、`/tmp/repro_seal_abort_spill`（源码与编译命令见 §9）。
- 最小修订：seal prologue 在锁内进入 `sealing` 状态；finalize 要求 `status==sealing`（abort 已置 aborted 即失败）；seal 前校验块覆盖 `0..total` 连续；abort 跟踪 `seal_in_flight` 并等其真实 I/O 完成后再 cleanup。

**M3-2（P1）abort 不排空在途 seal 写（M3-1 的独立缺陷）。**
- 位置：`storage.cpp:1633`（只有 `append_in_flight`），而 seal 的写由 `submit_write_batch` 在 `:1601-1607` 提交且不受 abort 跟踪。
- 期望：`FOUNDATION_API.md:197`/`:403`“abort_async 排空在途后撤销私有块”“真实 DMA/write 仍须等完成，不能为了返回取消提前释放 buffer”。实际：abort 返回 ok 时 seal 写可能仍在队列（构建 A 中 seal 随后 io_error；构建 B 中堆损坏）。
- 最小修订：与 M3-1 合并修；为 writer 增加 `pending_io` 计数，abort 的 cleanup 作为该计数的 continuation。

**M3-3（P1）未 sealed 的 writer 析构不清理，spill spool 永久泄漏。**
- 位置：`storage.cpp:340-386`（`WriterState` 无析构）、`:408-414`（`ArrayWriter` 无析构）；`cleanup_writer` 只在 abort（`:1649`）、begin 失败（`:1344`）、deferred abort（`:842`）被调用。
- 构型：spill writer、append 1 MiB、直接丢弃最后一个 `WriterRef`（不 seal/abort）→ writer 私有目录/文件保留。
- 期望：`OBJECTS.md:142`“任一未 sealed 析构等价 abort”；`FOUNDATION_API.md:403`（unlink 只试一次，失败留具名 orphan）。
- 最小修订：`~ArrayWriter` noexcept 调 `cleanup_writer`（status!=sealed），unlink 失败记 orphan/Issue。

**M3-4（P1）`buffer_bytes`/`metadata_bytes` 从不 claim；spill staging 未受额度约束、未按 ≤8 MiB 分块。**
- 位置：`storage.cpp:1304-1306`（只保存 buffer_bytes，用作 zstd 开关）、`:1325-1335`（spill 只 mkdir，不 charge）、`:662-674`（staging `make_shared` 未计账）、`:1428-1435`（一次 append 整段 payload）。
- 构型：`reserve(ctx,{host=1})` 后 `begin` spill `buffer_bytes=1MiB, metadata=1MiB` → ok 且 `claimed` 不变；append 4 MiB → `allocated` 不变。
- 期望：`OBJECTS.md:142`“spill 可用小于规范块的固定缓冲，raw 写与 zstd 流式解码/压缩均受实际缓冲额度约束”；`SCHEDULER.md:14` I/O chunk ≤8 MiB；`EXECUTION.md:91` std::vector/解压工作集必须计入准入。
- 最小修订：begin 时 claim buffer+metadata，seal/abort 释放；staging 按已预留 chunk 分块，禁止一次物化整段。

**M3-5（P1）已封存数组被全局 BlockStore 永久钉住，容量永不归还（M4 每次 Host 调用泄漏 26N+模型字节）。**
- 位置：`storage.cpp:1723-1733`（`register_array` 把 `lease` 存进 `entries_`，无删除/逐出 API）；`:1561-1576`（memory placement seal 注册）；`src/runtime/science_bridge.cpp:81-82`（桥接数组同样注册）。`SYSTEM.md:72` 的 cache 预算是 ≤H/8 且“逐出只影响性能”，这里无预算亦无逐出。
- 构型（已复现）：`append+seal` 一个 64 B memory array → 丢弃 writer/ArrayRef/arena 并 `close_command` 后 `snapshot`：`allocated=64, held=64` 仍不为 0（`peak=128`）。长跑进程会把 process ceiling 吃满，后续调用只能 `resource_exhausted`。
- 期望：`CALIBRATION_WALKTHROUGH.md:129`“已 sealed 数组在首次产品 publish 前仍属私有暂存，不能被 cache 永久钉住”；`SCHEDULER.md:44`“存活输出仍归该 scope，直至最后 lease 释放”（这里 lease 永不释放）；`EXECUTION.md:91` LRU/无界增长禁令。
- 最小修订：BlockStore 条目改为“内容缓存”，受独立预算与 pin/逐出策略管理；seal 的 Array 自身持有 Lease，cache 只持弱/可逐出引用；`close_command` 后未交付暂存应释放。

**M3-6（P2）读写账本单位与 cache 命中读量违反 EXECUTION:97。**
- 位置：`storage.cpp:818-823`（append 成功把 `appended*itemsize` 同时加到 `written_bytes` 与 `spill_written_bytes`，即未压缩 payload）、`:1164-1170`（read 按 `stored->values.size()` 计）、`:1003-1007`（cache 命中也 `ledger_account_bytes(...,byte_count,0,0)`）。
- 规范：`EXECUTION.md:97`“read/written 统计 Storage 边界实际字节，cache 命中读量为 0，重复物理 I/O 如实累加；spill_bytes 为累计写 spill 字节，written_bytes 包括它，不能相加当总写量”。
- 另：HEAD 新增的 `io01` 断言 `written + spilled >= bytes`（`validation/runtime/io01/io01.cpp:354-358`）把两者相加，恰好违反同一条“不能相加”的规则，因此该断言无法发现单位错误。
- 最小修订：spill 按 descriptor 的 stored_bytes 计；cache 命中不记 read；测试改判 written 与 spill 的包含关系，不相加。

**M3-7（P2）dyadic 只有写路径，没有读路径。**
- 位置：`storage.cpp:968-972`（`prepare_read` 对 `itemsize==0` 直接 unsupported）、`:1683-1697`（read_async 只实例化固定宽类型）；`memory.cpp:597-639` 也没有 `allocate/freeze<Dyadic>`。`append_async<Dyadic>` 在 `:1713` 存在。
- 期望：`FOUNDATION_API.md:199`“dyadic 使用单独精确编码读写，由 Storage 流式接口处理”。
- 实际：`read_async<Dyadic>` 无法链接（只有声明无定义）；公开 API 无法获得 `Block<Dyadic>`。
- 最小修订：补 streaming canonical-Dyadic JSON 读与 `allocate/freeze<Dyadic>`。

**M3-8（P2）append 不校验 `Block.model_hash`（“hash 来源”）。**
- 位置：`storage.cpp:1393-1425` 校验 dtype/itemsize/unit/index/连续性，从不看 `model_hash`；而 `read_async` 在 `:1184` 设置它。构型：writer u8{20}，先 append `model_hash=A` 再 append `model_hash=B` → 两次都 ok。规范：`FOUNDATION_API.md:197`“接收时验证…hash 来源”。

**M3-9（P2）NaN payload 未规范化为 quiet 位。**
- 位置：`storage.cpp:127-152`（append_le 原样 memcpy IEEE bits）。构型：`0x7ff8000000000001` 与 `0x7ff8000000000000` 得不同 `content_hash`。规范：`OBJECTS.md:140`“无值 NaN 在 FP64 统一 quiet 位 0x7ff8000000000000…”。-0 保留正确。

**M3-10（P2）`BlockStore::register_array` 覆盖同 id 条目会丢掉旧 Lease。**
- 位置：`storage.cpp:1728-1733`（`auto& entry = entries_[id]` 后 `entry.lease = std::move(lease)`）。构型：seal 两个内容相同的 1 MiB memory array → `allocated` 只 1 MiB 而非 2 MiB，且第一个 ArrayRef 仍存活时旧 claim 已被释放。规范：`FOUNDATION_API.md:193`“存活 claim 继续计费至最后 Lease 释放”。

**M3-11（P2）memory placement 的 `total*itemsize` 未做溢出检查。**
- 位置：`storage.cpp:1314`（`const U64 total_bytes = total * itemsize;`）。构型：f64 `shape{2^61}` → 乘积回绕为 0，跳过 `charge_vector`，begin 返回 ok 且 `allocated` 不增。规范：`OBJECTS.md:142` memory placement 必须完整预留；`FOUNDATION_API.md:199` `allocate` 对乘积检查。

---

## 6. M4 observation.calibrate

### 已核实的正确部分

- **唯一 kernel**：`src/observation/calibration_kernel.cpp` 的 `calibrate_sample`、`calibration_parameters`、`assemble_calibrated_native` 是唯一实现；同步入口 `calibrate.cpp:176,237` 与块路径 `calibration_signal.cpp:272,881` 都调用它们，没有第二份像素公式（`grep` 全树只有这一处公式）。
- **Host 全链可跑通**：我在 `/tmp` 重新编译 `validation/runtime/calibration/cal_host.cpp` 并运行 → **25/25**，与提交的 `observed_host.json` 的 checks/values 完全一致（仅 `resources.elapsed_seconds` 不同）；C-CAL-1 `[4,14,-8]`、C-CAL-2 `[0,10,-12]`、跨入口 `NativeObservation` identity 相同、共享根保留、DQ flat=0 得 NaN+bad+具名 Issue。
- **单位/index/DQ/区间**：`SignalBlock` 的 value/lower/upper 用 signal 单位、DQ 用 `model::unit_dimensionless()`（`calibration_signal.cpp:230-255`）；四个 buffer 都用 `data.index` 与输入 `model_hash`；区间由 `numerics::interval_*` 在同一表达式上向外传播（`calibration_kernel.cpp:112-178`）。
- **finish_native 的 science bridge scope**：`calibration_signal.cpp:630-632` 与 `calibration_process.cpp:211-213` 都选 `context.persistent ? persistent : progress`，模型数组计入 **R（persistent）**，没有记到 per-cohort F；R 的 `final_arrays=26N` 与 `begin_signal` 的 4 个 writer（`3*8N+2N`，`calibration_signal.cpp:405-419,434-435`）一致。**这一条不是缺陷。**
- **C-CAL 覆盖**：C-CAL-1/2/DQ/H/ORDER/ROOT/DEV 在 `validation/runtime/calibration/{evidence.md,cal_host.cpp,cal_kernel.cpp}` 有真实证据；C-CAL-H 的 N=200000、4 cohorts 在 Host 上实际完成。

### 发现

**M4-1（P1）同步 `calibrate_observation` 与 typed Host 对同一输入应用不同校验，结果 code 不同。**
- 位置：`src/observation/calibrate.cpp:66-72,91-152`（只查元素个数；`exposure_seconds < 0` 才算错；无 flat 时只要求 normalization 有限；不查 DQ 单位、不查 footprints）vs `src/observation/calibration_plan.cpp:141-168,203,222-241`（DQ 必须 `scale=="1" && linear_power==0`；exposure 必须 `>0`；无 flat 必须 normalization==1；footprints 必须有）。
- 构型（已复现）：同一 Raw/Calibration，DQ 数组 unit=ADU（其余合法）→ `acsd::calibrate_observation` 返回 `code=0`，`ProcessHost.invoke` 返回 `code=2`（`the raw quality array must be dimensionless`）。
- 期望：`COMPUTE_MODULES.md:58`、`DEVELOPMENT.md:65`、`CALIBRATION_WALKTHROUGH.md:129` 的“同一实现/所有入口同一 typed process”；`CALIBRATION_WALKTHROUGH.md:40` 要求 DQ 逐字段验证。
- 复现：`/tmp/repro_sync_host`（源码见 §9）→ `DIVERGENT=yes`。
- 最小修订：把 `validate_metadata` 提为唯一校验入口，`calibrate_observation` 直接调用；或让同步入口也走 `generate` 后的同一 `validate`。

**M4-2（P1）plan 的 work / R / F 与实际分配不符（多处漏计）。**
- 未计费的 O(N) `std::vector`：`calibration_signal.cpp:639-700` 的 `finish_native` 通过 `read_f64_values/read_u16_values` 复制 value/flags/flat/raw/bias（各 ~8N/2N），`calibration_kernel.cpp:212-262,312-330,406-420` 又复制 flat_values/raw/bias、构造 inverse_flat/shot/read/flat_jacobian（各 8N）。这些分配不经过 arena/ledger，N=200000 约 9–13 MB。规范：`EXECUTION.md:91`（std::vector/解压工作集必须计入准入）、`FOUNDATION_API.md:199`（“实际超计划先申请 grow，不能先 new 后补账”）。
- `kStorageWorkspaceUpper = 1 MiB` 是手填常数（`calibration_plan.cpp:38-43,358-361`），不是 `DEVELOPMENT.md:21` 要求的 `W_module(args)` 实测上界，也没有 allocator 日志。
- stage work 只声明 calibrate 段 n，read/write 段 `work_upper=0`（`calibration_plan.cpp:412-431`），但 Storage 的 read/append/seal/begin 都 `consume_work`；实测 N=3 一次调用 `usage.work=140`。d404 的 cal01 因 `max_work=1000` 在 C-CAL-ROOT 段的 invoke（累计第 5/6 次）永久等待（见 E-1），HEAD 的 cal01 把 `==count` 改成 `>=count`（见 E-3）。
- 最小修订：plan 把 Storage/codec 的 `work_units` 与所有 O(N) 临时向量的上界纳入 F/工作总量；用 allocator 实测替换常数；若 N 大时 `finish_native` 必须分块/流式，并计入该执行段。

**M4-3（P1）M4 的验收门槛 C-CAL-WRITE / C-CAL-CLEAN 未执行，且 C-CAL-WRITE 实际是 M3-1 的破坏路径。**
- `validation/runtime/calibration/evidence.md:75-83` 自述 “C-CAL-WRITE / C-CAL-CLEAN（short write、seal failure、cancellation in write）：require injected failures; the runtime verification cases own the failure-injection machinery”，即没有跑。
- `VERIFICATION.md:127` 明确把“四 writer 失败 seal、取消+science work 耗尽仍可 cleanup”列为 M4 实跑门槛；`CALIBRATION_WALKTHROUGH.md:149` 的 C-CAL-WRITE 期望“实际写结束前不释放 lease、无 Native/半成品”。M3-1 已证明该路径会交付 complete 半成品并在 spill 下 UAF。
- 最小修订：在修完 M3-1/M3-2/M3-3 后补 C-CAL-WRITE（第二数组短写、第四 seal 失败、取消在 write 提交后）与 C-CAL-CLEAN（已 cancelled + work 耗尽仍 cleanup），并接入 `run_cases.py`。

**M4-4（P2）plan 声明了两条 pipe edge/window，但 `stream_map_ordered` 完全不用 pipe/permit/credit。**
- 位置：`calibration_plan.cpp:435-451`（read_edge/write_edge，`ordered`、`max_items=window`、`max_queued_bytes`）；`src/observation/stream_map.cpp:119-186` 用 `std::deque<Task<Out>>` 限窗并发，sink 直接按 front 顺序，不调用 `make_pipe`、不取 publication permit、不按 edge 的 bytes 信用背压。
- 影响：`COMPUTE_MODULES.md:149` 要求“先由 plan 取得每 cohort 的 publication permit 与 arena”；plan 的 edge 字段成为死数据，ordered-pipe 契约（S-ORDER/PIPE01）在首个科学 process 未被行使。`window>1` 时多展开的 cohort 只靠 `block.arena`（F=单 cohort）承载，没有 E 预留，大 N 会 `resource_exhausted`/停放（`SCHEDULER.md:58,62`）。
- 最小修订：要么 `stream_map_ordered` 真正用 plan edge 建 pipe 并取 permit；要么在 plan 中不声明未使用的 pipe 字段，并在文档里把 window>1 标为未支持（带 E 准入）。

**M4-5（P2）C-CAL-ROOT 的“同根”证据是构造性必真；cal01 里唯一能查跨调用依赖的 check 被删除而非修复。**
- `validation/runtime/calibration/cal_host.cpp:522-558`：两次调用传入**同一个** `shared_root` 对象，再比较结果的 `shared_factors[i]->shared_id`。正确实现与“只会原样转发输入根”的错误实现都会通过；它不能证明“process 没有每次调用造新根”。
- `cal01` 在 d404 的失败项 `cal.root_shared_dependencies`（期望两次结果有共同 dependency id）在 HEAD 被从 `required_checks` 删除（35→34）；实际情况是 HEAD 的 `src/` 里没有任何 process 给 `Result.dependencies` 赋新值（`git grep HEAD -- src` 只有转发/空值，`src/runtime/host.cpp:860` 原样搬进 `WireResponse.dependencies`），所以 wire 的 dependencies 为空，与 `OBJECTS.md:16` 的 `dependencies[]`（具名输入对象版本/hash/角色）不符。
- 最小修订：C-CAL-ROOT 用两个**不同对象、相同 SharedId**的 NoiseModel 调用两次，比较完整 shared_id 集合与 root 的对象身份（而不仅是 id 字符串），并单独断言 Poisson 分支不新增随机根；`Result.dependencies` 由 adapter 填 input/request/master 身份，或明确从 wire 契约删除该字段并改文档。

---

## 7. 证据真实性（observed / comparison / 提交叙述）

| 编号 | 严重度 | 事实 | 证据 |
|---|---|---|---|
| E-1 | **P1** | `d404c99f7` 提交的 `cal01/comparison.json` 是 **fail**、0 checks；`observed.json` 是更早一次 attempt 的旧结果（24 checks、19 pass；其中 5 项失败含 DQ unit 与 root dependency），最终一次 attempt 没有产出 JSON。用 `d404` 源码重编 `cal01` 会在 C-CAL-ROOT 段的 invoke（此前 4 次 invoke 已通过）期间**永久挂起**（我把它跑满 120 s timeout），根因是 work 预算耗尽后的永久 `waiting_resources`（6fe0c8ac 已修）。与此同时 `m0-implementation-matrix.md:480-486` 写“M4 完成（C-CAL 全链）”。 | `git show d404c99f7:validation/runtime/cal01/{comparison,observed}.json`；`/tmp/cal01c`（用 `git show d404c99f7:.../cal01.cpp` 编译）在 `cal.root_second_adu_ok` 后挂起 |
| E-2 | **P1** | 同一个提交里 `validation/runtime/README.md` 的叙述（“pass 10 / fail 3 / pending 2”，fork01/order01 fail）与同提交的 `comparison.json`（fork01 pass 18/18、order01 pass 11/11、pipe01 fail、d01_on pending、cal01 fail → 12 pass/2 fail/1 pending）以及 `manifest.json`（只有 cal01 一条）互相矛盾。README 是旧叙述，重跑后未同步。 | `git show d404c99f7:validation/runtime/README.md`；同提交各 `comparison.json`；`manifest.json` |
| E-3 | **P1** | HEAD 的 15/15 快照在 `cal01` 上把必需判据从 35 条减到 34 条（删 `cal.root_shared_dependencies`，即 d404 的失败项）并把 `cal.cal1_usage_work` 从 `==count` 放宽为 `>=count`；`case.json` 只有一句“process usage aggregates stages”的注释，没有按 `README.md:31`“不得修改判据去迁就实现”给出缺陷/修订记录。`multi01.ratio_1_to_2` 的删除有代码注释说明（整窗含 admission 瞬态、保留 complete-round 判据），这一条我认为是合理的；`cal01` 的删除没有同等依据。 | `git diff d404c99f7 HEAD -- validation/runtime/cal01/{case.json,cal01.cpp}`；`git show HEAD:validation/runtime/README.md`（“不得修改判据去迁就实现”） |
| E-4 | P2 | `observed.json` 在“本次运行没有 JSON 输出”时不被清空：`pipe01`/`cal01` 的 `comparison.json` 是 0 checks/fail，但 `observed.json` 保留上一次的 19/21 checks。读者只看 observed 会误以为该次跑过。 | 同 E-1；`run_cases.py` 的失败写盘路径 |
| E-5 | P2 | `d404` 的 `manifest.json` 只含 `cal01` 一条（局部 run），却被当作整个包的结果；`validation/runtime/build_status.json` 与二进制在复核期间又被并行 agent 重编（00:04–00:13），与提交证据不是同一构建。 | `manifest.json`；`build/*.build.log` 时间戳 |
| E-6 | 正面 | 旧的 **7 pass / 4 pending** 未被改写：`validation/run_summary.json` 在 `ea984cdb` 与 `d404` 的 case 状态/functional 数逐项相同（只变了 `wall_seconds`）。`cal_host` 25/25 与提交 `observed_host.json` 的 checks/values 完全一致；`typed_smoke` 1255、`registry_negative` 107、`host_lifecycle` 66 可复现。 | `python` 对比脚本；本次实跑 |

**关于“M0—M4 完成”的声明**：`m0-implementation-matrix.md` 附录 A（480-486）在 `d404` 时把 M1/M2/M3/M4 标为“完成”，但同提交的 runtime 包是 fail/挂起（E-1/E-2）。到 `6fe0c8aac` 后 runtime 包 15/15，但本报告的 P0-1/P0-2、M2-2、M3-3/M3-4/M3-5、M4-1/M4-2/M4-3 均未被这 15 个 case 覆盖，附录 A 的“完成”仍超出现有证据范围。

---

## 8. 未被本轮验证的项（明确保留）

1. P0-2 spill 路径的 ASan 栈：`/tmp/acsd-build` 未用 ASan 插桩；我拿到的是 glibc 的 `malloc(): unsorted double linked list corrupted` 与 SIGSEGV。需要 ASan 全库构建再取栈。
2. C-CAL-WRITE / C-CAL-CLEAN 的修复后重跑；Host 的 spill placement 全链；`window>1` 且大 N 的 E 准入。
3. M5 其余 operation（Appendix A 自述 56 个具名 `unsupported`、`coadd.integrate` 为 `blocked_refactor`）逐项；M6 的 Normalize→Mosaic→Export 真实磁盘链路；Windows；真实 HST/M42。
4. 诊断 ON 的 `B9/B10`（summary 字段、独立诊断 I/O 预算）我只做了代码审读，未跑 ON 大负载。
5. `Arena`/Storage 的并发压测（除 P0-2 的 seal/abort 竞争外）；1e12、极区、SIP 等科学极值不在本轮。
6. 我未复核工作树里正在发生的未提交修改（截至 00:37 有 52 项，含 `task.hpp`/`pipe.cpp`/`memory.cpp`/`calibration_signal.cpp`）；若其中已修掉本报告的条目，需以新提交 + 新证据重新复核。

---

## 9. 复现命令（全部在 /tmp，不写 validation）

```bash
cd "/workspace/Astro CS Database"
INC="-I include"; LIBS="-L/tmp/acsd-build -Wl,--start-group -lruntime -lmodel -lnumerics -lstorage -lresources -lcodec -Wl,--end-group -lpthread -lzstd"

# M0：生成、manifest 覆盖、freeze 负例
python3 tools/generate_typed_bindings.py --check
python3 tools/check_design.py
ACSD_SOURCE_ROOT="$PWD" /tmp/acsd-build/acsd-runtime-manifest --output /tmp/manifest.json
python3 tools/check_process_coverage.py --manifest /tmp/manifest.json --source-root "$PWD"
/tmp/acsd-build/acsd_registry_negative ; /tmp/acsd-build/acsd_typed_smoke ; /tmp/acsd-build/acsd_host_lifecycle

# M0-2/M0-5：空 process 与共用文档都能 freeze
g++ -std=c++20 -O2 -I include /tmp/probe_empty_process.cpp $LIBS -o /tmp/pe && /tmp/pe
g++ -std=c++20 -O2 -I include /tmp/probe_dup_doc.cpp $LIBS -o /tmp/pd && /tmp/pd

# M1-1：work_upper=0 越 max_work
g++ -std=c++20 -O2 $INC /tmp/repro_work0.cpp $LIBS -o /tmp/repro_work0 && /tmp/repro_work0

# M2-1 P0：when_all 非法 Task
g++ -std=c++20 -O2 $INC /tmp/audit/when_all_probe.cpp $LIBS -o /tmp/wa && /tmp/wa

# M2-2 P1：ordered pipe 跨 cohort stall
g++ -std=c++20 -O2 $INC /tmp/audit/pipe_probe.cpp $LIBS -o /tmp/pp && /tmp/pp

# M2-4：broadcast 重复分支超 max_items
g++ -std=c++20 -O2 $INC /tmp/audit/broadcast_probe.cpp $LIBS -o /tmp/bp && /tmp/bp

# M3-1 P0：seal/abort 竞争（memory 与 spill）
g++ -std=c++20 -O2 $INC /tmp/repro_seal_abort.cpp $LIBS -o /tmp/sa && /tmp/sa
g++ -std=c++20 -O1 -g $INC /tmp/repro_seal_abort_spill.cpp $LIBS -o /tmp/sas && for i in 1 2 3 4; do /tmp/sas; echo "exit=$?"; done

# M3-5 P1：sealed array 被 BlockStore 永久钉住
g++ -std=c++20 -O2 $INC /tmp/repro_pin.cpp $LIBS -o /tmp/pin && /tmp/pin

# M4-1 P1：同步入口与 Host 校验分歧
SCI="-L/tmp/acsd-build -Wl,--start-group -lacsd_builtin -lruntime -lcodec -lworkflows -lphotometry -lsources -lcoadd -lsky -linformation -lprojection -lgeometry -lresponse -lresources -lstorage -lobservation -lcatalogue -luncertainty -loperators -lcoordinates -lnumerics -lmodel -Wl,--end-group -lpthread -lzstd"
g++ -std=c++20 -O2 -I include -I src /tmp/repro_sync_host.cpp $SCI -o /tmp/repro_sync_host && /tmp/repro_sync_host

# M4 正面：Host 全链 25/25
g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -I include -I validation/runtime/lib \
  validation/runtime/calibration/cal_host.cpp $SCI -o /tmp/calhost && /tmp/calhost > /tmp/calhost.json
# 与提交证据逐 check 对比（names/passes/values 应一致）
```

---

## 10. 最小修订顺序建议

1. **P0**：M3-1/M3-2（seal/abort 状态机 + 在途 I/O 排空）→ M2-1（when_all 非法 Task）。
2. **P1**：M4-1（唯一校验入口）→ M3-5（输出 Lease/缓存预算）→ M2-2（ordered frontier）→ M1-1（work_upper=0）→ M2-3（2P 窗口）→ M3-3（writer 析构清理）→ M3-4（buffer/metadata 计费）→ M4-2（W_module/临时向量/work 总量）→ M4-3（C-CAL-WRITE/CLEAN 证据）。
3. **P2**：M0-1/M0-2/M0-3/M0-4、M2-4…M2-9、M3-6…M3-11、M4-4/M4-5、E-4。
4. 修完后需重跑：`validation/runtime`（含新增的非法 Task、跨 cohort ordered、seal/abort 竞争、writer drop、`work_upper=0` 五个 fixture），`cal_host`/`cal_kernel` 全量，以及 `acsd_registry_negative`/`typed_smoke`/`host_lifecycle`；并在 audit matrix 附录 A 中把“完成”改为指向这些新证据。

---

### 附：本报告没有做的事

- 没有修改任何实现、头文件、验证用例或 git 历史；只在 `/tmp` 写探针与本报告。
- 没有把“测试通过/编译通过”当作语义通过的替代；每条 P0/P1 都给了文件行号、构型、期望/实际和复现。
- 没有复核未提交的工作树改动；没有覆盖 M5/M6、Windows、真实数据、GPU。

---

# 追记：修复后定向复核（2026-10-24 02:45）

> 本节是同一审核者对修复提交的**定向复核**，不改写 §1—§10 的历史结论（历史 REJECT 保留在原锚点 `d404c99f7`）。
> 复核锚点：`5329987b4`（HEAD；相对 `d4ccc8fa6` 只改 `docs/audit/m0-implementation-matrix.md`）。
> 运行时/科学代码自 `d4ccc8fa6` 起未变（`git diff d4ccc8fa6..HEAD -- src include` 为空）；M0—M4 相关代码在 `5b64e5cb0..d4ccc8fa6` 也无改动（只改 `validation/runtime`）。工作树在复核期间有未提交的 M5 改动（`src/coadd/*`、`src/catalogue/*` 等），**不在本追记范围**。
> 隔离复现：`git archive 5b64e5cb0 → /tmp/review-5b6`，用 `d4ccc8fa6` 的 `validation/` 覆盖（`src/include` 两提交相同），`cmake -G Ninja` 分别在 `ACSD_DEV_MODE=OFF/ON` 构建；`ctest` **19/19**；`validation/runtime` 全量 15 case；`cal_kernel`/`cal_equivalence`/`cal_host`/`calibration-faults` 脚本在 `/tmp` 树内运行。

## 追记 A：逐项关闭状态（原编号）

| 原编号 | 原严重度 | 复核状态 | 复验证据（隔离构建，锚点 `5329987b4`） |
|---|---|---|---|
| M0-1 模块/生成 descriptor 两套且缺 `runtime` 依赖 | P2 | **OPEN** | 导出的 manifest 仍为 `observation.calibrate.dependencies=[model,numerics,operators,uncertainty]`（生成表含 `runtime`）；M5 的 `descriptor_for` 未用于该 process |
| M0-2 零 operation process 可 freeze | P2 | **CLOSED** | `/tmp/probe_empty_process` → `freeze=invalid_model`；`reg01` 隔离构建 29/29 pass |
| M0-3 `then` 不要求 value / 非法 Task 报 success | P2 | **OPEN** | `task.hpp` 相关分支未改 |
| M0-4 `when_all` 失败合并不按 Code/Key 优先级 | P2 | **OPEN** | `task.hpp:286-292` 仍按输入序取第一个失败 |
| M0-5 共享 document 可 freeze | P2 | **CLOSED** | `/tmp/probe_dup_doc` → `freeze=identity_conflict`；`registry_negative` 112 checks（含两项新负例） |
| M1-1 `work_upper=0` 越 `max_work` | P1 | **CLOSED** | `/tmp/repro_work0`（隔离库）→ `consume_work(11)=resource_exhausted`、`scope.work=0` |
| M1-2 claim 的 arena/祖先进账非同一原子事务 | P2 | **OPEN** | 代码结构未变 |
| M1-3 `cpu_ns` 永空 / `metadata_bytes` 不计 | P2 | **PARTIAL** | 仍未计 metadata；cpu_ns 仍 null |
| M2-1 `when_all` 非法 Task UB/伪造成功 | P0 | **CLOSED** | `/tmp/audit/when_all_probe`（隔离库）→ `state=failed, code=1, size=0`；不再崩/不再伪造值 |
| M2-2 ordered pipe 跨 cohort 永久 stall | P1 | **CLOSED-WITH-CAVEAT** | `/tmp/repro_pipe_expect`：跨 cohort 先 park（有界），`detail::pipe_expect` 后按序投递；close(failed) 唤醒 parked sender（code=22）+ 接收端 `PipeEnd failed`。**但 FOUNDATION_API/SCHEDULER 正本未写 `pipe_expect/next_required_key`**（`git log` 最后一次改动仍是 `0a5dbfff3`）；契约只在 `pipe.hpp` 头注释，且仓库无执行器 fixture（Lead 已列为未验证项） |
| M2-3 无 2P ready window | P1 | **CLOSED** | `/tmp/repro_ready_window`（P=1 提交 100000）→ `pending=11, ok=11, resource_exhausted=99989, still_pending=0`；分批可行负载 8/8 ok |
| M2-4 broadcast 重复分支/组信用非原子 | P2 | **OPEN** | `/tmp/audit/broadcast_probe`：`broadcast(k,v,{p,p})` 仍让 1-slot pipe 产出两份；8 MiB DTO 仍过 64 B 窗口 |
| M2-5 metadata 不生效 / 未观察失败不记日志 | P2 | **OPEN** | `pool.cpp` task scope `metadata_bytes=0` 未改；`~TaskNode` 仍 default |
| M2-6 control 绕过 command CPU ceiling | P2 | **CLOSED（代码审读）** | `take_control` 已按 `command->cpu_running >= ceiling` 跳过并计 token |
| M2-7 I/O slot 不按 `min(P,4)`/2 clamp | P2 | **OPEN** | `read_slots_/write_slots_` 仍直接照抄 config |
| M2-8 重复 close 只比 kind+issue 数量 | P2 | **OPEN** | `pipe.cpp:531-532` 未改 |
| M2-9 无 `bounded_size` 登记，DTO 按 `sizeof` 计费 | P2 | **OPEN** | `pipe.hpp` 未改 |
| M2 S-MULTI 1:2 | P1 | **PARTIAL（证据不稳定）** | 见追记 B：隔离环境 16 次运行中 2 次 `98/202`（超 ±5 上界 201），14 次 `100/200`；安静环境 10 连跑全过。DRR 本身工作（dispatch 比例基本 2.0），判据/采样需修 |
| M3-1 seal 在 abort 后成功交付半成品 | P0 | **CLOSED** | `/tmp/repro_seal_abort` ×5：`abort=ok, seal=22(cancelled)`，无 `complete` 产品 |
| M3-2 abort 不排空在途 seal 写/UAF | P1 | **CLOSED** | 同 spill 变体 ×10：0 次崩溃；`abort=ok, seal=cancelled`；无 spool 残留 |
| M3-3 未 sealed writer 析构不清理 | P1 | **CLOSED** | `/tmp/repro_storage_life` A：spill writer 直接 drop → `after_drop_dirs=0`（begin 时 1） |
| M3-4 `buffer_bytes`/metadata 不计费、staging 不分块 | P1 | **OPEN（未实现）** | 同探针 B：spill `begin` 声明 `buffer=1 MiB, metadata=1 MiB` → `claimed_delta=0, allocated_delta=0`；`storage.cpp:1351-1352` 只保存字段，spill 分支只 `mkdir`（:1372-1381）。提交说明中的 “buffer/metadata staging claim” 未落地 |
| M3-5 sealed 数组被 BlockStore 永久钉住 | P1 | **CLOSED** | `/tmp/repro_pin`：丢弃 ArrayRef+`close_command` 后 `allocated=0, held=0`（`add_ref`/`release_ref` + Array deleter 生效） |
| M3-6 写账本按未压缩 payload / cache 命中记 read | P2 | **CLOSED** | `io01` 17/17：spill `written=spill=8,388,872`（stored bytes）；cache 命中分支已删除 read_bytes 记账（`storage.cpp:1054-1056`）。**测试仍把 written+spilled 相加**（`io01.cpp:354`），违反 EXECUTION:97（见 N-6） |
| M3-7 dyadic 无读路径 | P2 | **OPEN** | `storage.cpp:1015-1018` 仍 unsupported；无 `read_async<Dyadic>`/`allocate<Dyadic>` 实例化 |
| M3-8 append 不校验 `model_hash` | P2 | **CLOSED** | `storage.cpp:1459-1462` 绑定首个 model_hash，后续不一致拒绝 |
| M3-9 NaN payload 未规范化 | P2 | **PARTIAL / 回归** | NaN payload 已规范化；但 `-0.0` 被折叠成 `+0.0`（见 N-2），违反 OBJECTS:140 “保-0 位” |
| M3-10 同 id 覆盖丢旧 Lease | P2 | **WITHDRAWN** | 复核为 content-addressed 去重：物理只留一份、最后引用释放清账，不构成“存活输出提前回收”；原判过重 |
| M3-11 `total*itemsize` 溢出不检查 | P2 | **OPEN** | `storage.cpp:1361` 未改 |
| M4-1 同步/Host 校验分歧 | P1 | **CLOSED** | 唯一 `observation::validate_calibration_input`（`calibration_kernel.cpp:634`）；`cal_equivalence` 8/8（隔离重跑）；`/tmp/repro_sync_host` → 两端 `invalid_model`，`DIVERGENT=no` |
| M4-2 plan work/R/F 漏计 | P1 | **CLOSED-WITH-CAVEAT** | `plan_calibration`：N=3 → `R=33170, F=2166988, work=460`；N=200000 → `R=26832768, F=6623232, Σwork=268580`；`edges=0`、`window=2 → invalid_request`；`cal01` usage=140 ∈ [3,524]。**92N 物化向量只在 R 预留、不经 arena 计费**；`S(n)` 为公式上界，allocator 实测 14,849,040 B 只是支撑不是证明（evidence.md §3/§7 已声明） |
| M4-3 C-CAL-WRITE/CLEAN 未跑 | P1 | **CLOSED-WITH-CAVEAT** | `validation/process/calibration-faults` 隔离重跑 **16/16**（短写 io_error、seal 失败 io_error、写后取消 cancelled+deferred abort、spool 归零、无产品）。**M4 的 evidence.md:121 仍把 C-CAL-WRITE/CLEAN 列为未跑**（未同步）；注入在 runtime writer 组，校准 adapter 的 `begin_signal/seal_signal` 组合路径仍未直接故障注入 |
| M4-4 plan 声明未使用 pipe edge | P2 | **CLOSED** | edges=0、window>1 拒绝；`stream_map_ordered` 诚实串行 |
| M4-5 C-CAL-ROOT 证据构造性必真 | P2 | **PARTIAL** | `cal01` 现在传真实共享 `master_noise`（`shared_id=master_flat`, influence 1/1），但两帧仍传**同一个** NoiseModel 对象；等价的两次独立对象未测，P2 保留 |
| E-1 d404 的 cal01 失败/挂起与“M4 完成”矛盾 | P1 | **CLOSED** | 隔离套件 `cal01` 41/41 pass（严格判据、usage 140≤524、共享根）；`d4ccc` 提交快照 15/15 |
| E-2 README/comparison/manifest 三方不一致 | P1 | **PARTIAL** | README/comparison/manifest 现均为 15/15，但 `manifest.json.git_commit=5b64e5cb0`（比所在提交 `d4ccc8fa6` 落后一个），见 N-3 |
| E-3 cal01 判据被放宽 | P1 | **CLOSED** | `d4ccc` 恢复严格 required（37 checks）；`usage.work ∈ [3, plan-declared]` 是比 `>=` 更强的判据；未再删失败项 |
| 旧 7 pass / 4 pending baseline | — | **PRESERVED** | `validation/run_summary.json` 在 `ea984cdb` 与 `5329987b4` 的 case 状态/functional 逐项相同（只改 wall time） |
| M0 计数 | — | **REPRODUCED** | `registry_negative` 112、`typed_smoke` 1255（22/57/102 TypeId）、`host_lifecycle` 66、`ctest` 19/19、`design_integrity` pass |
| BigInt 符号修复（`ff6407689`） | — | **VERIFIED（范围内）** | 隔离 `ctest` 的 `numerics_selftest` pass，含 `dyadic_to_double(exact_point(-0.5))` 回归 |

## 追记 B：multi01 / S-MULTI 的判据建议（回答 Lead）

**事实**：隔离构建（与 `5329987b4` 的 M0—M4 代码相同）共运行 multi01 **16 次**：14 次 `count1=100,count2=200`（ratio 2.0）通过；**2 次为 `98/202`（ratio 2.061）**，后者 `202 > 2*98+5 = 201` 失败。两次离群都出现在我并行构建刚结束、机器有负载时；安静环境 10 连跑全过。提交的 observed 只保留通过的那一次。

**判断**：deficit RR 本身没有退回 1:1（14/16 精确 2.0），所以不是原 S-MULTI 缺陷复现；但当前 fixture **不能稳定证明**“每完整可行轮 1:2”，且判据与自身声明不一致：
- `case.json` 的 `tolerance` 字段写 `|count2-2*count1|<=8`，实际 check 用 `±5`；
- 判据是 **300 次 dispatch 的累计**比例，不是逐“完整可行轮”；累计窗口可能从/到非轮边界，允许个别轮次偏差被平均掉（也可能因边界被误判）；
- `observed` 只存 36 条 `order_prefix`，没有完整 300 条序列，无法独立复算“每轮 1:2”。

**建议（不放宽 S-MULTI）**：
1. **逐轮判据**：记录完整 dispatch 序列（300/更多条）；对每个“完整可行轮”（该轮 3 次 dispatch 前 `ready1>=1 && ready2>=2`）要求 multiset 恰为 `{w1,w2,w2}`；要求**违规轮次 = 0**、完整轮数 ≥100。这是 SCHEDULER“每完整可行轮 dispatch 比例 1:2”的直接检验，比累计 ±5 更严。
2. **采样对齐**：从首个完整轮开始、到最后一个完整轮结束，去掉预热与截断的半轮；或显式按轮累计（每轮 3 次）而不是“前 300 次”。
3. **重复性**：同一 fixture 在安静环境连续跑 **5 次**并要求 5/5（或至少 9/10），把每次计数保留在 attempts；单次 100/200 不能作为稳定证据。若保留累计判据，用已声明的 `±8`（相对 2.7%，仍比旧 `±4/120=3.3%` 更紧），不要为一次通过选 `±5`。
4. **P=2 合法性**：S-MULTI 未规定 P；P=2 使 2P=4 能容纳 1+2 个 ready，是合法可行环境，不算放宽。请在 `case.json` 写明“P=2 是判据的可行环境；P=1 的 2P 容纳不下完整轮，故不在 P=1 判权重”，并修掉 `tolerance`（±8）与 check（±5）的不一致。
5. **command ceiling**：`multi.command_concurrency_within_ceiling` 现在只断言两条 command 的并发 ≤`kThreads`(2)，应改为逐 command 记录 `ScopeLimits.ceiling.cpu_tokens` 并断言 ≤各自 ceiling（S-MULTI“单command同时运行不得超自己的cpu/io ceiling”）。
6. 若不做逐轮判据，则至少把 multi01 的“15/15”改为“稳定复现的通过次数/总次数”，不要以单次运行写 15/15。

## 追记 C：新发现（修复引入/仍存）

**N-1（P1，原 M3-4 未实现）spill 的 `buffer_bytes`/metadata 仍不计费、staging 不分块。**
- 位置：`src/runtime/storage.cpp:1351-1352`（只存字段）、`:1372-1381`（spill 只 `mkdir`）、`:1361`（memory 才 charge 全量）。
- 构型/实际：`reserve` 后 `begin` spill `buffer_bytes=1 MiB, metadata_bytes=1 MiB` → `claimed_delta=0, allocated_delta=0`；一次 append 仍整段 staging。
- 期望：`SCHEDULER.md:14`（I/O chunk ≤8 MiB）、`OBJECTS.md:142`（spill 受实际缓冲额度约束）、`EXECUTION.md:91`（staging/vector 计入准入）。
- 最小修订：begin 时 claim `buffer_bytes+metadata_bytes`，seal/abort 释放；append 按已预留 chunk 分块。

**N-2（P2，修复引入）`-0.0` 在封存身份中被折叠成 `+0.0`。**
- 位置：`src/runtime/storage.cpp:139-140`（f64；f32 在 `:150-151`）：`if ((bits & 0x7fff...) == 0) bits = 0;`。
- 构型/实际：seal `{+0.0}` 与 `{-0.0}` → 相同 `content_hash`（探针 `C +0==-0:1`）。
- 期望：`OBJECTS.md:140`“浮点保 -0 位”；旧实现保留 -0，本次 NaN 规范化把它一起折叠了。
- 最小修订：只规范化 NaN payload，保留 ±0 的原始位（删掉 zero 分支或把 `-0` 映射回 `0x8000…`）；并确认 `-NaN` 是否允许带符号（当前 `0xfff8…`，与“统一 quiet 位 `0x7ff8…`”的措辞需对齐）。
- 说明：科学值本身未被改写（只影响 hash/身份），影响面窄但属显式格式条款。

**N-3（P2）committed manifest 的 `git_commit` 落后一个提交。**
- `d4ccc8fa6` 提交的 `validation/runtime/manifest.json` 写 `git_commit=5b64e5cb0`（生成于 02:27，早于该提交）；读者会把它当成旧提交的证据。建议在提交前重新导出，或由 `run_cases.py` 在生成时记录 `HEAD` 并在提交前校验。

**N-4（P2）M4 evidence.md 未同步 C-CAL-WRITE/CLEAN。**
- `validation/runtime/calibration/evidence.md:121` 仍写“need injected failures… not exercised”；实际 `validation/process/calibration-faults` 16/16 通过。请把该证据链接进 M4 evidence，并注明注入在 runtime writer 组、adapter 组合路径未注入。

**N-5（P2）FOUNDATION_API/SCHEDULER 未写执行器 frontier 契约。**
- `pipe_expect` 只在 `include/acsd/runtime/pipe.hpp` 的 `detail` 命名空间 + 头注释；`FOUNDATION_API.md:231` 仍只说“ordered 返回 next_required_key”。M2-2 的正本条件未满足（见追记 A）。

**N-6（P2）io01 的写账本断言把 written 与 spill 相加。**
- `validation/runtime/io01/io01.cpp:354`：`written + spilled >= bytes`；`EXECUTION.md:97` 明确“spill_bytes 为累计写 spill 字节，written_bytes 包括它，不能相加当总写量”。当前实现 `written==spill==stored bytes`（一致），但断言本身是错的度量，会掩盖未来的重复计费。

**N-7（P2，范围外提示）19/19 ctest 只覆盖 12 个 driver。**
- `ctest -N`：3 个 calibration-faults + 9 个 process driver + `host_lifecycle`/`registry_negative` + 5 个核心测试（typed_smoke/numerics/operators/covariance/design）= 19。`geometry.drizzle`、`projection.export` 等目录只有 `observed_m5.jsonl`+README，不是 ctest 目标；M5 未关闭项与 Lead 的 Appendix B 一致，但“driver 全部进 ctest”应表述为“已存在的 12 个 driver 全部进 ctest”。

## 追记 D：判定更新（只针对 M0—M4；历史 REJECT 保留）

| 里程碑 | 原判定（d404） | 复核后（5329987b4） |
|---|---|---|
| M0 公共合同与登记 | ACCEPT（P2） | **ACCEPT**（剩 P2：M0-1 依赖、M0-3/M0-4 语义） |
| M1 账本/所有权 | ACCEPT-WITH-CAVEATS | **ACCEPT**（P2：claim 原子性、metadata/cpu_ns） |
| M2 Task/pool/pipe | REJECT | **ACCEPT-WITH-CAVEATS**（P0/P1 机制已闭；multi01 证据稳定性、pipe 正本未更新、P2 批次） |
| M3 异步 Storage/诊断 | REJECT | **未关闭**（P1 N-1 buffer/metadata 计费；P2 N-2 -0、M3-7 dyadic、M3-11 溢出；M3-6 测试度量） |
| M4 observation.calibrate | REJECT（作为“完成”） | **ACCEPT-WITH-CAVEATS**（P2：C-CAL evidence 未同步、adapter 级注入、C-CAL-ROOT 仍同对象、W_module 未 allocator 实测） |

**总体：ACCEPT-WITH-CAVEATS。** P0 已全部清零；唯一实质阻塞是 N-1（P1），N-2 属显式身份条款的 P2。若要求“所有里程碑 ACCEPT”，则 M3 保持 **REJECT**，直到 N-1 修复、N-2 修正并重新跑 `io01`/`b01`/`life01`/`cal_host` 与隔离 ASan 的 seal/abort 构型；M2 的 multi01 按追记 B 的逐轮判据 + 5 次重复通过后可去掉 caveat。

**建议的最小收尾顺序**：N-1（begin claim + staging 分块）→ N-2（-0 保留）→ N-3/N-4/N-5 文档/身份 → multi01 逐轮判据 + 重复运行 → 重跑 15 case、ctest、cal_* 与旧 7/4 baseline。完成后可给 M0—M4 **ACCEPT**。

## 追记 E：本轮复现命令（隔离，全部在 /tmp）

```bash
# 隔离树与构建（锚点 5329987b4 的 M0—M4 代码）
mkdir -p /tmp/review-5b6 && git archive 5b64e5cb0 | tar -x -C /tmp/review-5b6
git archive d4ccc8fa6 validation/runtime validation/process | tar -x -C /tmp/review-5b6
cmake -S /tmp/review-5b6 -B /tmp/review-5b6-build -G Ninja -DACSD_BUILD_SCIENCE=ON -DACSD_DEV_MODE=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/review-5b6-build -j 8 && (cd /tmp/review-5b6-build && ctest)
cmake -S /tmp/review-5b6 -B /tmp/review-5b6-dev -G Ninja -DACSD_BUILD_SCIENCE=ON -DACSD_DEV_MODE=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/review-5b6-dev -j 8 --target runtime model numerics storage
cd /tmp/review-5b6 && ACSD_BUILD=/tmp/review-5b6-build ACSD_DEV_BUILD=/tmp/review-5b6-dev \
  bash validation/runtime/build.sh && ACSD_BUILD=/tmp/review-5b6-build ACSD_DEV_BUILD=/tmp/review-5b6-dev \
  python3 validation/runtime/run_cases.py --no-build
# 定向探针（隔离库）
INC="-I /tmp/review-5b6/include"
RT="-L/tmp/review-5b6-build -Wl,--start-group -lruntime -lmodel -lnumerics -lstorage -lresources -lcodec -Wl,--end-group -lpthread -lzstd"
g++ -std=c++20 -O2 $INC /tmp/audit/when_all_probe.cpp $RT -o /tmp/f_wa && /tmp/f_wa
g++ -std=c++20 -O2 $INC /tmp/repro_work0.cpp $RT -o /tmp/f_w0 && /tmp/f_w0
g++ -std=c++20 -O2 $INC /tmp/repro_pin.cpp $RT -o /tmp/f_pin && /tmp/f_pin
g++ -std=c++20 -O2 $INC /tmp/repro_pipe_expect.cpp $RT -o /tmp/f_pe && /tmp/f_pe
g++ -std=c++20 -O2 $INC /tmp/repro_ready_window.cpp $RT -o /tmp/f_rw && /tmp/f_rw
g++ -std=c++20 -O2 $INC /tmp/repro_storage_life.cpp $RT -o /tmp/f_sl && /tmp/f_sl
g++ -std=c++20 -O2 $INC /tmp/repro_seal_abort.cpp $RT -o /tmp/f_sa && /tmp/f_sa
g++ -std=c++20 -O1 -g $INC /tmp/repro_seal_abort_spill.cpp $RT -o /tmp/f_sas && /tmp/f_sas
# M4
SCI="-L/tmp/review-5b6-build -Wl,--start-group -lacsd_builtin -lruntime -lcodec -lworkflows -lphotometry -lsources -lcoadd -lsky -linformation -lprojection -lgeometry -lresponse -lresources -lstorage -lobservation -lcatalogue -luncertainty -loperators -lcoordinates -lnumerics -lmodel -Wl,--end-group -lpthread -lzstd"
g++ -std=c++20 -O2 -I /tmp/review-5b6/include -I /tmp/review-5b6/src /tmp/repro_sync_host.cpp $SCI -o /tmp/f_sync && /tmp/f_sync
g++ -std=c++20 -O2 -I /tmp/review-5b6/include -I /tmp/review-5b6/src /tmp/repro_plan_numbers.cpp \
  -L/tmp/review-5b6-build -Wl,--start-group -lobservation -loperators -luncertainty -lnumerics -lmodel -Wl,--end-group -lpthread -lzstd -o /tmp/f_plan && /tmp/f_plan
cd /tmp/review-5b6 && export ACSD_BUILD=/tmp/review-5b6-build
bash validation/runtime/calibration/run_cal_equivalence.sh   # 8/8
bash validation/runtime/calibration/run_cal_kernel.sh       # 37/37
bash validation/runtime/calibration/run_cal_host.sh         # 33/33
g++ -std=c++20 -O2 -I include validation/process/calibration-faults/calibration_faults.cpp $RT -o /tmp/f_faults && /tmp/f_faults  # 16/16
# multi01 重复运行（示例 10 次，安静环境）
for i in $(seq 1 10); do timeout 180 validation/runtime/build/multi01; done
```

---

**追记签署**：reviewer-m0m4（未参与实现）。本追记只记录复核证据与建议，不修改实现/验证用例，不 commit/push；历史 §1—§10 的 REJECT 仍是对 `d404c99f7` 的结论。

---

# 追记二：最终复核（2026-10-24 13:45，锚点 `5d643e07e`）

> 历史 §1—§10 的 REJECT 与追记一的结论保留；本节只按新锚点更新状态。相对追记一（`5329987b4`），M0—M4 运行时代码变化为：`types.hpp` 新增 `InvocationContext.dispatch_sequence`、`pool.cpp` 两处派发赋值、`storage.cpp` 的 staging claim 与 `-0/NaN` 修复（`0a3975a1b`）；调度器另有 `8895df9a9`/`8ec1e947f`。M5/M6 与 photometry 的 x01/Armijo 不在本轮 M0—M4 范围。

## 1. M2：S-MULTI 关闭（撤销追记一的 caveat）

新 fixture 的判据是**滑动可行窗口**：每个 `i, i+1, i+2` 三次 dispatch 起点都满足 `ready1≥1 && running1<ceiling1 && ready2≥2 && running2<ceiling2` 时，`commands[i..i+2]` 的多重集必须恰为 `{w1,w2,w2}`；可行窗口 ≥100、违规 = 0、5/5。排序用池在派发时赋值的 `ctx.dispatch_sequence`（不是 callable 入口墙钟顺序），queued 硬上限 4（2P ready 窗口），避免 deferred 被计成 ready。

**隔离复现（`/tmp/review-5d6`，OFF+DEV 两套构建）**：完整套件 **16/16 pass**（含 `storage01`、`d01_on` DEV）；`multi01` 单独 **5 次独立调用 × 每次 5 run**，全部 41/41 checks、每 run 可行窗口 **5,811–10,266**、**违规 0**、window 拒绝 0、per-command 并发 ≤ `ceiling.cpu_tokens=2`；committed observed 的 5 run 为 9,573–11,359、违规 0。判据没有放宽：逐可行窗口的严格 `{1,2,2}` 比旧的累计 ±5 更强；P=2 是合法可行环境（2P=4 才放得下 1+2），并已在 `case.json` 的 `why_P2` 写明。

**残余 P2（测量机制，不阻塞）**：`ready1/ready2/running1/running2` 是 fixture 在 **callable 入口**用自身计数器采样的，而排序键是池在**选中**时赋的 `dispatch_sequence`；P=2 下选中与入口之间有时间差，可行性分类因此是近似的。25 次运行未出现假通过（0 违规），但若要严格到“派发时刻的 ready 快照”，应由池在派发记录里一并记录 ready/ceiling，或只使用在选中时更新的计数器。另：`case.json` 的 `runs=5` 与外部 5 次调用是两层；请在证据里保留全部 5 次外部 attempt，而不是只留最后一次的 observed。

## 2. M3：staging claim 与 -0/NaN 已修，留两个 P2

修复实际在 **`0a3975a1b`**（不是 `04af17cb7`；后者是 LocalStorage 行读、catalogue XP null 与 sky 默认 patch 的 covariance 崩溃）。隔离探针（`/tmp/final_storage_life`）：

- **staging claim**：spill `begin(buffer=1 MiB, metadata=64 KiB)` → `claimed_delta = 1,114,112 B`（=1 MiB+64 KiB），不再为 0；`abort` 后 claim 仍在（见下），WriterRef 释放后回到 0。
- **-0/NaN**：`seal({+0.0})` 与 `seal({-0.0})` 的 `content_hash` **不同**（`+0!=-0:1`）；payload/quiet/signalling NaN 归一到 `0x7ff8000000000000` 后 **相同**（`nanA==nanB:1`）。b01 的 ASan 构型（`-fsanitize=address`）覆盖 ±0/NaN 并 pass（30/30）。
- **writer drop**：未 sealed 的 spill writer 直接析构 → spool 目录 `after_drop_dirs=0`。
- `life01/io01/b01` 以 ASan 构建并运行 pass（build.sh 显式 `-fsanitize=address`），`storage01` 16/16 套件内 pass。

**残余 P2**：
- **claim 释放时机**：`cleanup_writer`（`storage.cpp:467`）不重置 `staging_claim/metadata_claim`；`abort` 后只要 WriterRef 还活着，claim 就一直计费（我的探针：abort 后 claimed 仍为 1,114,112，drop WriterRef 后才 0）。提交说明写“cleanup releases them”，实际是 WriterState 析构时由成员释放。建议在 seal/abort 的 cleanup 里显式 reset。
- **append 超出声明 staging**：spill writer 声明 `buffer_bytes=1 MiB`，一次 append 4 MiB 的 block **被接受**（`append=0`）；`encode_append` 仍按整块物化，claim 只预留了 `min(buffer_bytes,8 MiB)=1 MiB`。提交说明“encode staging 被 reservation 界定”只在调用方按 buffer 大小时成立。建议：append block > `buffer_bytes` 时拒绝（具名 `resource_exhausted`）或内部按 ≤buffer 分块；这也是 OBJECTS:142“受实际缓冲额度约束”的直接要求。
- 仍 OPEN：`read_async<Dyadic>` 无路径（`storage.cpp:1012`）；`total*itemsize` 溢出未查（`:1376`）。

**M3 结论**：由“未关闭”升级为 **ACCEPT-WITH-CAVEATS**——原 P1（完全不 claim）已实质修复，且 ASan/回归证据齐；上述两点 P2 不影响 M0—M4 结论，但若要求严格的 OBJECTS:142 缓冲约束，请先修 append 上限。

## 3. M4：caveat 保留（与 Lead 一致）

隔离重跑：`cal_equivalence` 8/8、`cal_kernel` 37/37、`cal_host` 33/33、`calibration-faults` 16/16；`cal01` 在 16/16 套件内 pass（严格 required，usage 140≤524，共享 `master_noise` 根）。**保留 caveat**：(a) plan 的 R/F/W 是声明上界，不是 allocator 逐分配实测；(b) C-CAL-WRITE/CLEAN 只在 runtime writer 组注入，校准 adapter 的 `begin_signal/seal_signal` 组合路径未注入；(c) C-CAL-ROOT 仍把同一个 NoiseModel 对象传给两帧，等价独立对象未测。

## 4. M0/M1 与正本同步

M0/M1 结论不变（ACCEPT，P2 列表同追记一）。**N-5 已关闭**：`FOUNDATION_API.md` 现在同时记录 `dispatch_sequence`（池派发时赋的单调序号，P>1 排序用）与执行器 `expect(key)` frontier 契约（含“单调不减、回退 invalid_request”）。由此产生一个新的 **P2 文档/实现不一致**：`pipe_expect_core`（`pipe.cpp:370-373`）对回退 key 直接返回 ok，不报 `invalid_request`；探针：`send item0` 后 `expect(item0)` → `code=0`（文档要求 `invalid_request`）；`expect` after close 正确返回 `invalid_request`。建议实现补上回退拒绝，或把文档改为“回退被忽略”。

## 5. 证据一致性（P2，发布前清理）

- `validation/runtime/README.md:26` 与 `:101` 仍写 **“15 pass / 1 fail（multi01）”**，但同文件逐 case 表把 multi01 记为 pass，committed `manifest.json` 是 **16/16**；三方不一致。
- committed `manifest.json.git_commit = 8ec1e947f`，而所在 HEAD 是 `5d643e07e`（README 说它“记录本次运行的 HEAD”，实际落后一个提交）。
- `io01` 的 `io.written_ledger_at_seal` 仍存 `written+spilled`（check 已改成 `written>=bytes`，值只是冗余，建议删掉以免误读）。
- `observation.calibrate` 的 manifest 依赖仍缺 `runtime`（M0-1 未动）。
- 观察：我第一次 `build.sh && run_cases.py` 时 reg01 报过一次 “did not emit JSON evidence”，立即重跑即 29/29 pass；属构建/启动竞态，未复现，不记为缺陷。

## 6. 最终分项结论

| 里程碑 | 结论 |
|---|---|
| M0 公共合同与登记 | **ACCEPT**（P2：M0-1 依赖、M0-3/M0-4 语义） |
| M1 账本/所有权 | **ACCEPT**（P2：claim 原子性、metadata/cpu_ns） |
| M2 Task/pool/pipe | **ACCEPT**（S-MULTI 滑动可行窗口 5×5 复现、0 违规；P2：入口采样机制、`pipe_expect` 回退文档/实现不一致、M2-4/5/7/8/9） |
| M3 异步 Storage/诊断 | **ACCEPT-WITH-CAVEATS**（staging claim、-0/NaN、ASan 证据已闭；P2：claim 释放时机、append 超出声明 buffer、dyadic 读、溢出） |
| M4 observation.calibrate | **ACCEPT-WITH-CAVEATS**（plan 声明上界、adapter 注入、C-CAL-ROOT 同对象） |

**总体：ACCEPT-WITH-CAVEATS。** 没有 P0/P1 阻塞；M4 的两项 caveat 是 Lead 已明确保留的声明上界/注入范围，M3 只剩 P2，M2 的 S-MULTI 已从“不可测/不稳定”变为可复现的严格判据。发布前建议按 §5 清理 README/manifest 一致性，并按 §2 修 claim 释放与 append 上限（若要求严格 OBJECTS:142）；`pipe_expect` 回退按 §4 二选一补实现或改文档。

## 7. 复现命令（`/tmp`，与报告其余部分一致）

```bash
mkdir -p /tmp/review-5d6 && git archive 5d643e07e | tar -x -C /tmp/review-5d6
cmake -S /tmp/review-5d6 -B /tmp/review-5d6-build -G Ninja -DACSD_BUILD_SCIENCE=ON -DACSD_DEV_MODE=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/review-5d6-build -j 8 && (cd /tmp/review-5d6-build && ctest)     # 19/19
cmake -S /tmp/review-5d6 -B /tmp/review-5d6-dev -G Ninja -DACSD_BUILD_SCIENCE=ON -DACSD_DEV_MODE=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/review-5d6-dev -j 8 --target runtime model numerics storage
cd /tmp/review-5d6
ACSD_BUILD=/tmp/review-5d6-build ACSD_DEV_BUILD=/tmp/review-5d6-dev bash validation/runtime/build.sh
ACSD_BUILD=/tmp/review-5d6-build ACSD_DEV_BUILD=/tmp/review-5d6-dev python3 validation/runtime/run_cases.py --no-build   # 16/16
for i in 1 2 3 4 5; do timeout 300 validation/runtime/build/multi01 > /tmp/multi_$i.json; done   # 5 外部调用 × 5 run
g++ -std=c++20 -O2 -I include /tmp/final_storage_life.cpp -L/tmp/review-5d6-build -Wl,--start-group -lruntime -lmodel -lnumerics -lstorage -lresources -lcodec -Wl,--end-group -lpthread -lzstd -o /tmp/g_storage && /tmp/g_storage
g++ -std=c++20 -O2 -I include /tmp/final_pipe_expect.cpp -L/tmp/review-5d6-build -Wl,--start-group -lruntime -lmodel -lnumerics -lstorage -lresources -lcodec -Wl,--end-group -lpthread -lzstd -o /tmp/g_pe && /tmp/g_pe
bash validation/runtime/calibration/run_cal_equivalence.sh   # 8/8
bash validation/runtime/calibration/run_cal_kernel.sh       # 37/37
bash validation/runtime/calibration/run_cal_host.sh         # 33/33
```

**追记二签署**：reviewer-m0m4。仅记录复核证据与建议；不修改实现/验证用例，不 commit/push。
