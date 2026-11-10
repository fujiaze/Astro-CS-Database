# Runtime/资源/Storage 修复的独立复核（review of runtime-fix-report）

> 复核者：runtime-auditor（未参与修复实现；本轮只新建本文件，未改 `src/` 或他人 docs）。
> 被复核报告：`docs/audit/runtime-fix-report.md`（实施者自报；本文件不采信其结论，只采信我自己的构建、探针与代码路径）。
> 复核对象：`rebuild/acsd-v2` 当前 HEAD **`0f8e362f5`**（其中 `b600b9e2d` 已把 P0-3 的 CLI 接线提交；`1f48c58ad` 为 product 的 signal_only/no_noise 改动；`77a13c89c` 含 runtime 修复本体）。修复报告写“HEAD 77a13c89c、commands.cpp 未提交”**已过时**，见 §1。
> 构建：`cmake --build build/science --target acsd -j16`（`acsd` sha256 `18f8b6de…`，2026-11-10 21:38:55）；依赖库为同一工作树。全量 `cmake --build build/science` 当前会在 `check_design` 失败（他人未提交的 `include/acsd/model.hpp` 新增 `apply_sky(...,Id frame)` 与生成 schema 不同步），与 runtime 无关，见 §7.3。
> 独立探针：`/tmp/rt-verify/verify.cpp`（我自己的实现，链接 `build/science/*.a` + 公共头 + `runtime/internal.hpp`，不改仓库），日志 `full10.out`；`/tmp/rt-verify/reopen.cpp`（产品重开）。CLI 判别实验在 `/tmp/rt-verify/cli/{h1,pfull,psig}`。**未使用**实施者的 `/tmp/rt-fix/*` 输出作为证据。

## 0. 复核总表

| 项 | 实施者声明 | 我的复核 | 证据 |
|---|---|---|---|
| **P0-3** 用户 Limits 成为唯一账本 | 关闭 | **部分关闭**：host/work/cpu 成立；**spill 被 workflow 夹成 0**，用户 `spill_bytes` 不可达；contract 驱动仍 1 GiB | §2.1、§4-R1/R2 |
| **P0-4** broadcast 取消唤醒 + 有界 | 关闭 | **主路径关闭**；仍有取消/停放竞态窗口 | §2.2、§4-R4 |
| **P0-5** 删除第二套线程池 | 关闭 | **关闭**（探针与 CLI 线程数一致） | §2.3 |
| **P0-6** bridge 先 claim 后 alloc 再 commit | 关闭 | **关闭**（含 reserve commit/peak；隐藏 vector 仍不计，属诚实边界） | §2.4 |
| P1-1 append 累计数 | 关闭 | **关闭**（524288） | §3 |
| P1-2 溢出检查 | 关闭（写域） | **关闭**（begin 溢出 `resource_exhausted`；read/LocalStorage 侧代码核对） | §3 |
| P1-3 spill 配额/活存 | 关闭 | **Storage 层关闭**（零配额拒绝、活存/peak 计费、释放归还）；接线层被 R1 破坏 | §3、§4-R1 |
| P1-4 cleanup orphan | 关闭 | **关闭**（真实写失败+只读 spool → orphan JSONL + 账本保留） | §3 |
| P1-5 发布持久化 | 部分 | **部分**：`LocalStorage::write_file` 已 fdatasync+目录 fsync；product 发布链仍无 | §4-R5 |
| P1-6 canonical 块 | 未修（写域外） | **未修**（整数组单块、id=array oid） | §4-R6 |
| P1-7 path 根约束 | LocalStorage 侧关闭 | **LocalStorage 关闭**（3 个穿越探针拒绝）；product 侧未修 | §3、§4-R6 |
| P1-8 有界读/解压 | 关闭 | **关闭**（代码核对：精确长度、2 GiB 上限、zstd frame 一致性、bad_alloc 具名） | §3 |
| P1-9 未知 recipe | 未修 | **未修**（仍 identity） | §4-R7 |
| P1-10 read_reserved | 未修 | **未修**（1 MiB arena 读 4 MiB → `resource_exhausted`） | §3 |
| P1-11 Dyadic 读 | 未实现（给设计） | **未实现** | §4-R8 |
| P1-12 waiting 有界 | 关闭 | **代码关闭**（`max(64,8P)`；未做压力实验） | §3 |
| P1-13 close_command 等在途 I/O | 关闭（保守） | **实现正确（503 ms），但生产路径无调用者** | §4-R9 |
| P1-14 ingest 计费 | 未修（写域外） | **未修**（4.2 MB 分配、账本全 0） | §3 |
| P1-15 block_index 生命周期 | 关闭 | **关闭**（1/0） | §3 |
| P1-16 Unit 默认初始化 | 未修（头） | **未修**（头未改；本轮复现为“偶然通过”，仍是 UB） | §4-R10 |
| P1-17 normalize status 合并 | 未修（写域外） | **已由 HEAD 修复**（`merged_termination/merged_recipe`） | §3 |
| P1-18 事件流写失败 | 未修 | **未修** | §4-R11 |
| 产品 P0-1/2（Lead 降级为 unknown+omitted） | 部分 | **降级已生效，但仍有“假 complete”** | §2.5 |

一句话：**P0-4/5/6 可判关闭；P0-3 只关闭了 host/work/cpu，spill 与 contract 路径未闭合；产品侧仍会发布 `profile=full + complete=true` 但缺 noise 闭包；P1 的写域内修复大体成立，但 P1-13 未被生产调用、P1-16/9/10/11/14/18 仍开。**

## 1. 复核身份与构件

- 复核期间 HEAD 从 `77a13c89c` 前移到 `0f8e362f5`（三次提交：`b600b9e2d` CLI 单账本接线、`1f48c58ad` product signal_only/no_noise、`0f8e362f5` export BUNIT）。修复报告的“commands.cpp 仍未提交”**不再成立**：`git status` 中 `apps/cli/commands.cpp` 已干净，`b600b9e2d` 正是那 81 行。
- `make_services` 采纳（adoption）后新增的语义：`src/runtime/pool.cpp:1463` 在已有活跃 `Services` 时直接返回同一实例；`Services::create` 的 `identity_conflict` 分支（`:466`）现在只在直接调用内部 create 时可达。这与 `FOUNDATION_API:381`“第二次创建活跃独立池返回 identity_conflict”的字面不同，属于**需要正本裁决的语义变更**（见 §4-R12），不是 bug 本身。
- 冻结用例（仅作回归观察、不作正确性证明）：`acsd_host_lifecycle` 66 checks PASS、`acsd_typed_smoke` 1255 checks PASS（我这边的运行，二进制 `18f8b6de`）。

## 2. P0 逐项复核（我的实验，不是读报告）

### 2.1 P0-3：用户 Limits 是否唯一账本

**成立的部分**（`/tmp/rt-verify/verify.cpp` S0，用户 Limits = 32 MiB / 1 thread / 1e7 work / 8 MiB spill）：

```
threads at start: 1
first make_services: ok cpu_threads=1 host=33554432 spill=8388608 max_work=10000000
threads after first: 4 (expect main + Limits.threads + 2 io)
second make_services(1GiB/2/4e9): ok same_instance=1 cpu_threads=1 host=33554432
threads after second: 4
open_command(1GiB/2/4e9) under 32MiB: ok
effective command ceiling: host=33554432 spill=0 cpu=1 max_work=10000000
```

- workflow 的 1 GiB/2 tokens/4e9 work 只被**夹紧**，不能放宽 ✓（`narrow_limits` `src/runtime/memory.cpp:70-95`；`open_command` 的 token 夹紧 `src/runtime/pool.cpp:686`）。
- CLI 低 H 具名拒绝（F2，最新二进制 `18f8b6de`）：`resources.bytes=1 MiB` → `exit=4`，`frame_finished ... "R+F cannot be admitted at the declared minimum block size and placement; the plan must reduce its working set or the request must fail before run"`，且**没有生成 products 目录**；`256 MiB` → `exit=0` 已发布。

**不成立的部分（新发现，P0-3 残留）**：

1. **spill 维度被夹成 0。** workflow 打开的 command 使用默认构造的 `ScopeLimits`（`src/workflows/normalize.cpp:381-386`、`normalize_runtime.hpp:338-341`），只填 host/cpu/work；`narrow_limits` 取 `min(spill, parent)` → `0`。实测即使进程 ceiling 的 spill 是 8 MiB，workflow command 的 spill 仍是 0：
   ```
   adopted command ceiling: host=33554432 spill=0 cpu=1 max_work=10000000 (user process spill=8388608)
   ```
   零是“真实零”（API 语义），因此**用户配置 `resources.spill_bytes` 在 CLI 生产路径上不可达**：任何需要 spill 的重规划都会在 `begin` 拿到 `resource_exhausted`，而不是按用户的 spill 额度落盘。P0-3 的“H/threads/work”成立，“spill”不成立。
2. **contract 驱动（`acsd-contract`）没有单账本接线。** `apps/contract/driver.cpp:1711` 只创建 `CpuResources`（现在是 facade），从不创建 `Services`；workflow 第一次 `make_services` 用硬编码 1 GiB 创建账本。因此独立驱动/公开 API 路径的用户 `limits` 仍未进入 runtime 账本。这是 CLI 之外的第二个生产入口。
3. 报告 §2 的 `/tmp/rt-fix/adopt` 输出与我一致，但它没有覆盖“workflow command 的 spill=0”这一层（只测了 open_command 的 host/cpu/work）。

### 2.2 P0-4：broadcast 取消唤醒与有界

**主路径关闭**（`verify` S3，两条 `max_items=1` edge）：

```
F1 key0=success key1(parked)=waiting_dependencies terminal=0
P0-4 extra parked=63 refused=137 (cap=max(64,8*P)=64)
F1 after cancel+400ms: parked=cancelled terminal=1
close: ok/ok parked=cancelled
```

- 取消后 400 ms 内 `cancelled` ✓；停放窗口 64（`pipe.cpp:696-709`），超限 `resource_exhausted` ✓；`try_fulfill_broadcasts` 对已取消组直接 cancelled（`:240-246`）✓。
- **残留竞态（未观测到，但由代码交错可证）**：`pipe_broadcast_erased` 在**取 `broadcast_mutex()` 之前**检查 `cancellation_requested(ctx.cancel)`（`pipe.cpp:660-670`），随后才加锁并 push（`:710`）。交错：broadcast 检查为 false → `request()` 置位并跑完所有 pipe watcher（此时组还不在全局表）→ broadcast 加锁 push → 该组没有任何 watcher 再被调用，`wait_external` 仍会挂起。`try_fulfill_broadcasts` 只在 receive/close 调用，不覆盖该窗口。修法：把取消检查移到全局锁内，或 push 后在锁内复查并立即唤醒；这是 P2 级窄窗口，但属于“取消必然终态”的不变量缺口。

### 2.3 P0-5：唯一 CPU 池

**关闭。** `CpuResources` 构造函数已无线程（`src/resources/cpu_resources.cpp:45-46`），`parallel_for` 改为调用线程内联（`:109-120`）；`grep std::thread src` 只剩 pool 的 `hardware_concurrency/get_id` 与注释（worker 线程池由 `Services` 唯一创建）。CLI 实测（`threads=1` 的 256 MiB normalize）稳态 `Threads: 4`（main + 1 CPU lane + 2 I/O lane；stdin 为 /dev/null 时控制线程立即退出），即 lane 数 = `Limits.threads + 2` ✓。无第二 CPU 池。
注意：`CpuResources` 仍保留自己的 `reserve/consume_work/usage` 计数器，但**没有科学调用者**；它不 gate 任何任务，`Usage` 数值不再代表 runtime 账本（观测陷阱，非缺陷）。

### 2.4 P0-6：bridge 计费

**关闭**（`verify` S1）：

```
P0-6 bridge copy(4MiB): ok alloc_during_copy=4241336 (data vector itself=4194304)
bridge-after-copy host(held=8388608 claimed=0 alloc=4194304 peak=4194304)
P0-6 BridgeResources::reserve(1MiB): ok
bridge-reserve-held host(... alloc=5242880 peak=5242880)
bridge-reserve-released host(... alloc=4194304 peak=5242880)
P0-6 BridgeResources::reserve(64MiB > arena): resource_exhausted
P0-6 copy(4MiB) with 1MiB arena: resource_exhausted alloc_during_denied=8 (expect ~0, not 4MiB)
```

- claim→alloc→commit 顺序成立（拒绝时只多分配 8 字节，不是 4 MiB）；`reserve` commit 进 allocated/peak、释放归还、超 arena 拒绝 ✓。
- **诚实边界**：模块内部 `std::vector/matrix/MPFR` 的隐藏分配仍无 allocator hook（报告 §5.3 已承认）；`peak_payload_bytes` 对“经 bridge 的数组 + 模块声明的 workspace”为真，对隐藏分配仍不是 M5 封顶证明。这不是本次修复的退步，但不能把 P0-6 的关闭外推为“真实峰值已证明”。

### 2.5 产品 P0-1/P0-2 的“降级”复核（Lead 要求确认没有假 complete）

用最新二进制跑同一帧（128×128，H=256 MiB），分别请求 `profile=full` 与 `profile=signal_only`，发布后逐项读取 manifest/对象：

```
# pfull（profile=full）
manifest: complete=True profile=full
assessment: complete=False science=conditional noise=unknown execution=complete
caps: [...,"omitted:no_noise: the covariance action is not reopenable: the product carries no LinearAction reader"]
estimate assessment: {'complete': True, 'science': 'bounded', 'noise': 'unknown', 'capability': 'supported'} noise_ref=None error_ref=None
staging manifest present: ['manifest.staging.json']

# psig（profile=signal_only）
manifest: complete=True profile=signal_only
assessment: complete=False science=conditional noise=unknown
caps: [...,"omitted:no_noise: ..."]
estimate assessment: {'complete': True, 'science': 'bounded', 'noise': 'unknown', 'capability': 'supported'} noise_ref=None
```

结论：

- **降级本身生效**：product assessment 已变成 `complete=false / science=conditional / noise=unknown`，遗漏以 `omitted:no_noise:...` 具名记录 ✓。
- **仍有“假 complete”**：
  1. 顶层 `manifest.complete=True` 与 `profile=full` 同时出现（`src/workflows/product.cpp:997` 仍无条件 `manifest.complete = true`；`:893` 只记录 profile，不校验 profile↔闭包）。按 EXECUTION:141，full 的 required closure 未满足时 `complete` 必须为 false；这里把“signal 可读”当成“产品 complete”。
  2. 产品内 `SignalEstimate` 的 assessment 是 `complete=true / science=bounded / capability=supported`，而 `noise` 为空且**没有任何 issue**；OBJECTS 要求能力名 Ref 空缺必须有对应 unavailable issue，实际只有 manifest 的 free-text capability 提到它。
  3. 重开（`reopen` 探针）：`signal/support/bounds/response/error` 恢复，`noise=0`、assessment `noise=unknown`；但 `decode_signal_estimate`（`product_io.cpp:734-741`）与 `ProductReader::estimate` 仍不恢复 noise，遗漏也没有进入 estimate 的 issues。若从该产品再发布，`collect_signal` 只在 collect 失败时补 `omitted`（`product.cpp:293-299`），`noise=null` 不会触发 → **遗漏痕迹会在下一级产品丢失**（本轮 export 因 projection 的既存错误未能复跑，代码路径足以判定）。
- 因此：**对 X01 改 config 为 signal_only 是对的方向，但不能替代 `complete/profile` 的强制一致性**；`full + omitted → complete=false（或拒绝发布）` 仍必须修。
- `manifest.staging.json` 仍留在发布目录（F10，未修）。

## 3. P1（写域内）复核结果

以下为我的探针输出（`full10.out`）或代码核对，均已在上表给出裁决：

| ID | 我的观察 |
|---|---|
| P1-1 | `append(524288 f64, 1MiB chunks): ok elements=524288`（修复前 17039360）✓ |
| P1-2 | `begin(shape 2^61 f64): resource_exhausted`（分配前拒绝）✓；read cache 的三处乘法与 `LocalStorage::open` 的 `count*item` 均改为 checked（代码核对 `storage.cpp:1157-1180,1215-1232`、`local_storage.cpp:300-330`） |
| P1-3 | `begin(spill) with zero spill quota: resource_exhausted`；有配额时 `seal ok`，`spill(held=4MiB alloc=4MiB peak=4MiB)`；释放后 `alloc=0`、`peak` 保留 ✓（但见 §4-R1） |
| P1-4 | 真实写失败（部分写后 chmod 只读、跨 canonical 块触发新 tmp 文件）：`append=io_error seal=io_error abort=ok`；`runtime-orphans.jsonl` 记录 `{"path":...,"bytes":1048576,"reason":"Permission denied"}`，账本 `allocated spill=4MiB`（声明容量）✓ |
| P1-8 | `open` 按 descriptor 推导精确文件长度，`read_file_bounded` 先 `file_size==expected`，`decompress` 校验 `ZSTD_getFrameContentSize` 与 2 GiB 上限；`bad_alloc` → 具名错误（代码核对）✓ |
| P1-10 | `read 4MiB with 1MiB arena: resource_exhausted`（公共 `Storage` 仍无 `read_reserved`）✗ |
| P1-12 | `command->waiting` 上限 `max(64,8P)`、超限释放 execution claim 并 `resource_exhausted`（`pool.cpp:861-884`，代码核对；未做压力实验） |
| P1-13 | 我直接提交 500 ms I/O 后调 `close_command`：`ok elapsed=503 ms` ✓；但生产路径**无调用者**（§4-R9） |
| P1-14 | `ingest_array: ok alloc=4197764`，账本前后全 0 ✗ |
| P1-15 | `find_block before/after last Array release: 1/0` ✓ |
| P1-17 | 已提交进 HEAD（`normalize.cpp:958-1063` 的 status/recipe 合并）✓ |
| P1-7 | `../secret.bin`、绝对路径、`blocks/../../secret.bin` 全部 `invalid_request`；`blocks/good.bin` `ok` ✓（LocalStorage 侧） |

## 4. 仍未修 / 新发现（按严重度）

**R1（P0-3 残留，P0 级）workflow 的 spill 被夹成 0。** `src/workflows/normalize.cpp:381-386`、`normalize_runtime.hpp:338-341` 的 `ScopeLimits` 只填 host/cpu/work，`spill_bytes` 默认 0；`narrow_limits` 取 min → 0。后果：用户 `resources.spill_bytes` 在 CLI 路径不可达，需要 spill 的低 H 场景直接 `resource_exhausted`。修法（Lead/workflows）：从 `context.resources` 的 Limits（或 `remaining_limits(parent)`）填 `spill_bytes`；然后用 H=8 MiB / spill=64 MiB 的真实验证“spill 而不是偷偷成功”。

**R2（P0-3 残留，P0 级）contract 驱动没有单账本接线。** `apps/contract/driver.cpp:1711` 只建 `CpuResources`；workflow 的首次 `make_services` 用硬编码 1 GiB。修法：driver 与 CLI 一样先按请求 `limits` 建 `Services`（或给 workflows 传入 Limits），否则 §6.2 的“H 生效”只对 CLI 成立。

**R3（P0-1 残留，P0 级）`full + omitted` 仍 `complete=true`。** `product.cpp:997` 无条件置真；`:893` 不校验 profile。修法：`manifest.complete = required_closure_satisfied(profile)`；full 缺 noise → 拒绝或显式降为 signal_only 并记录“为何降级”，不能同时保留 `profile=full`。

**R4（P0-4 残留，P2）取消/停放竞态。** `pipe.cpp:660-670` 的取消检查在 `broadcast_mutex()` 之外；watcher 扫过之后仍可 push 一个组。修法：锁内复查或 push 后唤醒。

**R5（P1-5 残留）product 发布链无 fsync。** `LocalStorage::write_file` 已 `fdatasync` + 目录 `fsync`（`local_storage.cpp:109-127,461-470`）；但 `src/workflows/product.cpp:86-102` 的私有 `write_file` 仍只 `flush`，`rename(staging,destination)`（`:1006`）前没有文件/目录持久化。**R5 应作为 P0-1 发布语义的一部分一起修**。

**R6（P1-6/P1-7 残留）产品侧块与路径。** 实测 manifest 的每个数组是一个整块（`begin=0,count=全数组`），`block.id` = `acsd.Array:<oid>` 而不是 `canonical_block_id`；`product.cpp:360,1082` 仍直接 `root / path`，未用 `resolve_within`。`open_product` 侧需要同样的根内约束。

**R7（P1-9）未知 recipe 仍静默 identity。** `product_io.cpp:828-830`。`version` 仍未校验。

**R8（P1-11）Dyadic typed read 仍未实现。** `storage.cpp:1129-1133`、`local_storage.cpp:309-311` 仍 `unsupported`；报告给的是设计，不是可运行证据。

**R9（P1-13 接线）`close_command` 无生产调用者。** `grep -rn close_command src apps` 只有定义与注释；CLI 退出靠 `Services::shutdown` 的 I/O join。修复本身正确并通过我的 503 ms 实验，但它在生产链上是死代码——要么 workflow/CLI 显式调用，要么在文档里明确“只供嵌入式调用者”。

**R10（P1-16）`Unit` 头未改。** `include/acsd/model.hpp:38` 仍无默认成员初始化；`unit_equal`（`storage.cpp:72-76`）仍比较未初始化值。我的“默认构造 ArrayLayout.unit”实验这次偶然 `ok`（栈恰好为 0），**这是 UB，不是修复**；不能据此标关闭。修法必须在头里（或强制所有构造点 `Unit{}`）。

**R11（P1-18）事件流写失败仍被忽略。** `apps/cli/events.cpp:53,100` 只 `flush`，不检查 `stream_`；磁盘满时可静默截断事件流而退出码仍 0。

**R12（API 语义变更，需正本裁决）`make_services` 采纳。** `pool.cpp:1463` 在已有活跃 Services 时返回同一实例并**忽略调用者 PoolConfig**，而 `FOUNDATION_API:381` 写的是“第二次创建活跃独立池返回 `identity_conflict`”。这是实现 P0-3 的合理机制，但需要：更新正本措辞（adopt 语义），或在 API 上给出可检测的“adopted”状态，避免调用者以为拿到了自己的预算。当前冻结用例没有覆盖这个分支（`host_lifecycle` 只有一个 fixture）。

**R13（P2，本次复核暴露）CLI 统计口径不一致。** h1 失败帧：`frames[0].status="failed"`，但 summary 是 `unsolved=1, failed=0`（`apps/cli/commands.cpp:1242-1275` 把所有非 cancelled 的未解帧都计入 unsolved）。退出码 4 正确，但 GUI/读者会看到自相矛盾的分母。

**R14（P2，可复现性）staging 目录名冲突。** `scratch_directory()` 用进程内 `g_writer_sequence`（`storage.cpp:454-462`），同一 `scratch_root` 遗留的 `writer-N`（尤其只读 orphan）会被新进程复用；`create_directories` 对已存在目录成功，于是写进旧目录。我的第一次探针就因此得到 `append=io_error`（清理 spool 后正常）。建议 PID/随机后缀或独占创建 + 启动孤儿扫描。

## 5. F1–F11 复跑对照（我的结果）

| 故障 | 我的复跑 | 结论 |
|---|---|---|
| F1 broadcast 取消挂起/无界 | 取消后 400 ms `cancelled`；63 parked + 137 refused | 关闭（窄竞态见 R4） |
| F2 H=1 MiB 偷偷成功 | `exit=4`、具名 admission 失败、无 products | 关闭（CLI 路径） |
| F3 full 产品缺 covariance 仍 complete | `complete=True profile=full` + `omitted:no_noise`；estimate complete/supported 无 issue；重开 noise=null | **未关闭**（R3） |
| F4 append 累计数 | 524288（原 17039360） | 关闭 |
| F5 写失败+只读 spool 残留 | orphan JSONL + 账本保留 4 MiB | 关闭 |
| F6 无 read_reserved | 1 MiB arena 读 4 MiB → `resource_exhausted` | 未修（R8 同族） |
| F7 ingest 4.2 MB 账本 0 | `alloc=4197764`、账本 0 | 未修 |
| F8 Unit 未初始化 | 本次“偶然 ok”，头未改 | 未修（R10） |
| F9 未知 recipe→identity | `product_io.cpp:828-830` | 未修（R7） |
| F10 残留 staging manifest | pfull/psig 均有 | 未修 |
| F11 无 fsync 发布 | `LocalStorage::write_file` 已修；product 链无 | 部分（R5） |

## 6. 建议（给 Lead 的修复顺序）

1. **R1 + R2**：workflow 的 `ScopeLimits.spill_bytes` 从用户/父域取值；contract driver 也建唯一 Services。然后用真实低 H + spill 配置证明“spill 或具名拒绝”，而不是 0 配额下的拒绝。
2. **R3 + R5**：`manifest.complete` 与 profile/闭包强制一致；product 私有 `write_file` 走 fdatasync+目录 fsync（复用 `local_storage` 的 `sync_handle`），再 rename。
3. **R4**：broadcast 取消检查移入 `broadcast_mutex()`（或 push 后锁内复查唤醒）。
4. **R6/R7/R9**：product 的 canonical 块/路径约束、未知 recipe 拒绝；决定 `close_command` 是否由 workflow/CLI 调用。
5. 其余 P1（R8 Dyadic、P1-10 read_reserved、P1-14 ingest、R10 Unit、R11 events、R13/R14 可观测性/暂存命名）按 owner 分派。
6. **R12**：更新 FOUNDATION_API 的 `make_services` 语义（adopt）或加显式 adopt 标志；否则“唯一账本”的实现与正本措辞不一致。

## 7. 复核边界与未证明项

1. 我未跑 ASan/UBSan/TSan、未做 SIGKILL/掉电注入；R4 的竞态是代码交错推理，未在实际运行中观测到。
2. 我未复跑完整 8 帧 X01 全链（约 40 分钟）；CLI 判别实验是单帧 `x01-00`。
3. `cmake --build build/science`（all）在当前工作树会因 `check_design` 失败（他人未提交的 `include/acsd/model.hpp` 与生成 schema 不同步）；`--target acsd`/`acsd-contract` 可构建。该状态不属于 runtime 修复，但会让 CI-free 的“构建通过”声明随工作树变化，需要 Lead 在合并前确认 schema 重新生成。
4. 我观察到另一个与本轮无关的长时 `acsd normalize`（PID 1064468，20:38 启动、单线程持续 CPU），未确认其是否死循环；不属于本次修复的判据，仅提示并行 worktree 上存在未清理的旧进程。
5. P1-13 的 drain 是进程级 `io_pending`（报告已注明保守）；我验证了机制，但没有验证在真实 workflow 中的顺序收益（因为无调用者）。
