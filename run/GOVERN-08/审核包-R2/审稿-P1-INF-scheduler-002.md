# 审稿 P1 — INF-scheduler-002（第 1 遍 · 对抗性）

- 片号：`INF-scheduler-002`
- 层：`lib/infrastructure/scheduler`
- 仓库 HEAD：**实测 `1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`**（派单文本写的是 `850a9ede`，二者不一致；本报告以工作树实际内容为准并如实记录）
- 审核日：按派单口径，未核实「检查通过」机制为通过；全部结论由本人重读原文 + 亲自构造反例 + 独立取证得出

---

## 1. 读完了吗

**成员份数 14 / 读完 14 / 成员总行数 4040 / 实读 4040 行 / 覆盖率 100%。未读完的：无。**

口径说明：
- 「成员份数/总行数」取自权威片清单 `run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml` 的 `INF-scheduler-002` 块（`成员份数: 14`、`实际行数: 4040`）。
- 「实读行数」= 我用 `read` 工具对 14 个文件各自从第 1 行读到末行的行数之和，逐文件与 `wc -l` 对齐，合计 4040（与清单完全一致，无缺口）。
- 为判定上下文（类型定义、枚举值、是否活调用者）另读了 4 个**不属于本片**的头/文件，均在文中标出出处、不计入本片覆盖率：`lib/include/acsd/core/context.h`、`lib/include/acsd/core/trace.h`、`lib/include/acsd/core/plan_estimator.h`、`lib/include/acsd/core/executor.h`、`lib/include/acsd/core/pipeline.h`。
- 另读了 `eng/tests/runtime/test_rt005_plan_estimator.py` 的 C 节（199–267 行）作为**线索核实**，结论不依赖它。
- 未跑编译 / ctest / pytest / 构建 / 任何仓内二进制。唯一执行动作是在 `/tmp/g08_ce/` 下的独立自写片段（非仓内、非仓内二进制）。

---

## 2. 本片判定

**判定：需修（且含 3 条阻断级）。**

最重 3 条：

1. **冻结峰值判据是自洽式断言，且可被构造成恒红门。** `plan_estimator.cpp:512` 的 `peak_within_frozen_bounds` 与其唯一调用方测试 `test_rt005_plan_estimator.py:206` 是**同一个公式的两份拷贝**，测试在 `:245` 直接断言两者 `lo2==lo && hi2==hi`（要求逐位相等），并在 `:204-205` 自承「用与 peak_within_frozen_bounds **相同的**公式…两处实现同源错误 → 双 FAIL」—— 把它称作「双保险」。同一来源的错误必然双绿，这不是保险。更严重：该界的结构是 **P3 专属**（`out2 + cache_all(C²·1MiB) + read/8 + 2MiB`），却无条件套用到 P1/P2；P2 的主导项是**线性**样本栈 `C·F·1MiB`，于是当 `F ≳ C+18` 时界失效。我已构造并**执行验证**：8 tiles × 27 frames → peak 217 MiB > max 211 MiB 判红；1 tile × 21 frames → 22 > 21.12 判红。而测试自身取样是 100 tiles × 8 frames，落在绿区深处 —— **恰好避开了界失效的区间**（筛掉真信号）。

2. **`12u << (2*order)` 32 位回绕/UB，使 phase2 heavy 计划退化成 0 工作量。** `plan_estimator.cpp:301`，`12u` 是 32 位 `unsigned int`；守卫 `:289` 只挡 `input_order > kHipsMaxOrder(=20, plan_estimator.h:44)`。已执行验证：`order=15 → cells=0`（静默回绕）；`order=16..20 → UB`（移位量 ≥32）。后果 `work_units=0`、`max_useful_workers=0`（低于 `min_useful_workers=1`，不变量倒挂）、read/write=0、peak=1 MiB，而 `estimate_plan` 仍返回 `ok`。该 0 经 `runtime.cpp:206` 进入 `spec.estimated_memory_bytes`，让内存回压按 ~1 MiB 预留，而真实域是 order 15 的约 1.3e10 个 cell。**最坏的那条输入被折成 0。**

3. **内存回压在「节点独跑」时整体失效。** `scheduler.cpp:202` 的条件是 `memory_limit_bytes_ > 0 && active.load() > 0`；`active==0` 时走 `:231-235` **无条件取队首，完全不做内存判定**。因此任意单独运行的节点可远超 `memory_limit_bytes_`，上限对单节点从不被强制。并且 `runtime.cpp:212` 在不可估算时把 `estimated_memory_bytes` 留 0 —— `runtime.cpp:199` 的注释原文是「硬写 0 会让内存回压恒不触发 —— 0+0 <= limit 恒真」，而该失效在不可估算分支**原样复现**。

---

## 3. 逐文件清单

| # | 文件（`文件:行`） | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `lib/infrastructure/scheduler/src/export_stream.cpp`（688 行，全读） | 全部 14 个成员文件之一 | `:612` 注释称「checksum 由 writer 在行主序下累积；此处按同一算法独立复算以便核对」—— 实测 writer 只算字节数 `written`（`:432`/`:500`），**全文件无任何 checksum 累积**；`o.checksum` 写一次（`:629`）后**全仓无消费者**。`:624-625` 读失败 `break` **无错误码**，产出只覆盖文件前缀的 hash 却当作完整值上报。`:604 (void)pre_cancelled` 丢弃 `:577` 的计算。`:120-123` 与 `:130-133` 输出两个同名同值 JSON 键。`:107`/`:137` 返回值全丢。 | 须修（M1/S17/S18/S19/S20） |
| 2 | `lib/infrastructure/scheduler/src/plan_estimator.cpp`（573 行，全读） | 同上 | 自洽式断言 `peak_within_frozen_bounds`（`:512-543`）已如上。`:301` 32 位移位回绕/UB。`:220 plane_bytes*2` 无溢出守卫，而 `:522` 同类计算有守卫。`:127` NaN 静默走缺省，与 `:151`「无静默默认」政策矛盾。`:284`/`:306` 的越界守卫只覆盖 `n_tiles_input`，`:301` 的回绕不在其中。`:547-567` JSON 无转义。`:533`/`:534` 无推导经验常数。 | 阻断（B1/B2） |
| 3 | `lib/infrastructure/scheduler/src/context.cpp`（554 行，全读） | 同上 | `detect_repeated_calls`（`:404-435`）**不按 run_id 过滤**（`:410-414`），而 `same_call_site`（`:366`）含 run_id ⇒ 跨 run 误报 repeated-call；规则 (b) 按 node_id 计数，**忽略 entry 与 module_id** ⇒ 同节点合法调两个模块被误判。`:466-468` 的 RUNNING 掩盖 `:474-476` 的 ERROR。`:264-280` 字段类型错误一律折成缺省且仍 `return true`。`:456-476` 与 `:492-509` 行处理块逐字复制两份。`:204`/`:549` `noexcept` 工厂 + `make_shared`。`:149` 全局计数无下溢保护。 | 须修（M5/M6/M7/M17/S22/S23/S24/S25/S33） |
| 4 | `lib/infrastructure/scheduler/src/runtime.cpp`（490 行，全读） | 同上 | `:78-82 catch(...) { return m; }` 静默吞掉配置解析失败，无错误码；`:205-208` 仍标 `plan_estimated=true`。`:185-196` `plan()` 失败静默退化到 `:181-182` 的 `(1,budget_)` 占位。`:281-292` 用**永不复位的进程级高水位** `peak_lease` 算 `node_granted`，且 `acquired_total` 是全局计数、并发节点互相污染。`:278`/`:328` `call_count = 1` 是写死常量却以观测身份写进 trace。`:159-161` 无生产者则依赖边被静默丢弃。`:84-93` u64 把负数/错类型折成 0。`:463` `cancelled_` 无读取者。`:455/456` 成员序使 `governor_` 先于 `scheduler_` 析构。 | 须修（M4/M5/M19/M20/M21/S16/S17） |
| 5 | `lib/infrastructure/scheduler/src/scheduler.cpp`（409 行，全读） | 同上 | `:202` 内存回压在 `active==0` 时整体豁免。`:75` 用 `std::queue` 但未 include `<queue>`。`:287` 节点无 `fn` 时 `node_ok` 保持 true ⇒ **静默标 COMPLETED**。`:360-364` + `:379-383` 任一失败即停全部推进，与注释「若其依赖链在失败下游」不符。`:277-278` heavy 闸门在占用 active 槽之后才获取。`:37 add_node` 同 id 静默覆盖。 | 阻断（B3）+ 须修 |
| 6 | `lib/infrastructure/scheduler/src/executor.cpp`（342 行，全读） | 同上 | `:19-20`「本文件是全仓唯一 executor 池实现；scheduler 不再自建 std::thread 池」被**同片内两处证伪**。`:31` `g_task_exceptions` 自称「唯一留痕面」，实测**只写不读**。`:139` `granted_workers = budget->budget()` 把配置值当观测。`:184-191` enqueue 返回 void、取消时静默丢任务。`:106-113` 取任务后遇取消 → 任务静默丢弃，而 `wait_all` 因 `cancelled` 直接返回。`:266-271` I/O 侧异常**一条 trace 都不写**。`:260` `IoTaskClass` 被丢弃。`:94-105` 预算为 null 时 worker 永久阻塞。`:321/331` noexcept + make_unique。 | 须修（M2/M3/M11/M12/M13/M14/M17/S21） |
| 7 | `lib/infrastructure/scheduler/src/block_frame.cpp`（260 行，全读） | 同上 | `:108-115` 注释宣称「消费不存在的块：消费者声明的输入块必须有生产者」，实际 `produced` 集合建好后**从未被使用**，循环体只有 `(void)c;` —— 声称的判据不存在。`:191-206` `consume` 的 bool 三义（成功但存活/未声明消费者/状态非 CREATED 全 false）。`:150-161` 五条拒绝路径全返回裸 nullptr，无稳定错误码。`:153` 先乘后判，溢出发生在 `:154` 上限检查**之前**。`:163-173` find 对死块与不存在返回同一个 nullptr。`:216` erase 后调用方裸指针悬空；全类无互斥。 | 须修（M8/M9/M10 + S8） |
| 8 | `lib/infrastructure/scheduler/core/phase_lifecycle.py`（259 行，全读） | 同上 | `:189-191 assert_no_shared_registry` **`return True` 恒真门**，docstring 承诺的检查从未读 `_global_seen`，且**全仓无调用者**。`:137-142 read_only_external` **恒 `return True`**，而 `test_phase_lifecycle.py:160` 正是在断言这个常量。`:163-171 assert_registry_view_isolated` **自洽式**：视图由 `:78` 的同一谓词过滤而成，再从**同一文件**读回同一谓词比较 ⇒ `leaks` 结构上恒空，CLI `:224-235` 恒打印 PASS。`:200 if p is not None` 使全未注册图 `phases=[]` ⇒ PASS。`:28`/`:30` 两个正则定义后从未使用。`:63` 每次调用重读重解析磁盘。`:241-242` 未知名也 `return 0`。 | 建议（S1–S7，均为自洽/恒真门族） |
| 9 | `lib/infrastructure/scheduler/src/artifact_store.cpp`（178 行，全读） | 同上 | `:18` 只查长度不查字符集，文案却写 "must be 64 hex"，64 个 'z' 通过；文件头 `:1` 的「篡改检测」建立其上。`:109-110` `from_json` **从不调用 `validate()`**。`:91`/`:95` 对 `coordinate`/`invalids` 做无范围检查 `static_cast`。`:65-71` 未知 role 静默落 UNKNOWN，`:31` 的 `artifact_role_name` 无逆向映射、往返不闭合。`:48` `to_json` 在 `out==nullptr` 时仍 `return true`。 | 须修（M22/M23 + S10 关联） |
| 10 | `lib/infrastructure/scheduler/src/logging.cpp`（120 行，全读） | 同上 | `:35-38`/`:112-114` 手搓 JSON 无转义，含 `"`/反斜杠/**换行**的日志产出非法 JSONL，换行还会把一条记录劈成两行，被 `context.cpp:445-484` 逐行解析判成两条 skipped。`buf[512]`/`buf[128]` **静默截断**超长消息且无截断标记。`:64 ++seq_`/`:70 ++emitted_` 裸自增而 `Logger::log` **全程无锁**，同文件 `:74` MetricsAggregator 却加锁。`:77 sum_ += value` 无溢出检查、从不记 min。`<mutex>` 未 include。 | 须修（M15/M16 + S10/S15） |
| 11 | `lib/infrastructure/scheduler/src/cpu_budget.cpp`（67 行，全读） | 同上 | `:63-64` 无租约时返回**整机预算**。`:46` 依赖生成头 `runtime_resources_generated.h`。`:57` cap==0 静默表示无上限。`:63 max(1u, ...)` 把 0 租约转成 1。`:34 sizeof(cpu_set_t)` >1024 核平台静默退回 `hardware_concurrency`（全机核数而非亲和核数）。 | 建议（S8/S9 + 见 S31/S32） |
| 12 | `lib/infrastructure/scheduler/src/cpu_budget.h`（42 行，全读） | 同上 | `:4` 引 `docs/engineering/THREADING_MODEL.md` 作**首要依据**，该文件**不存在**。`:12-13` 自称「调用方 fail-closed：只收紧不放大」，但 `:14-16` 的分支 ② 恰恰是放大；这是**诚实写明的刻意设计**，我不把它当隐藏缺陷，但「不取租约即获全机并行」的后果需登记。 | 悬空引用（M18）+ 建议 |
| 13 | `lib/infrastructure/scheduler/src/executor_runtime.h`（39 行，全读） | 同上 | 逐条核实其声明：`:36 shared_work_executor` 的定义**确实存在**（`module_adapters.cpp:303`）；`:23` 声称的 `#include "executor.cpp"` 编入方式**也确实存在**（`module_adapters.cpp:257`）。**两条声明均属实，不构成发现** —— 我原本准备记为悬空，核实后否决。文件本身无缺陷。 | 通过（核实后无发现） |
| 14 | `lib/infrastructure/scheduler/src/build_stamp.cpp`（21 行，全读） | 同上 | 全文 21 行只做编译期常量装配。依赖构建期生成头 `build_stamp_generated.h` 与 `eng/tools/gen_build_stamp.py`（**均存在，已核实**），语义权威 `docs/engineering/VERSIONING.md`（**存在，已核实**）。无线程池、无静默降级、无溢出、无悬空引用。 | 通过 |

---

## 4. 发现清单

### 4.1 阻断（3）

**B1｜自洽式断言：冻结峰值判据与被检量同源，且可构造成恒红门**
- 位置：`plan_estimator.cpp:512-543`（被检判据）、`plan_estimator.cpp:239`（被检量 `peak_memory_bytes` 的来源）、`eng/tests/runtime/test_rt005_plan_estimator.py:206-245`（唯一调用方，自称「独立复算」）
- 同源性证据：测试 `:204-205` 原文「本文件用与 peak_within_frozen_bounds **相同的**独立公式复算峰值…两处实现同源错误 → 双 FAIL，非同源则至少一处拦截」；`:245` `CHECK(lo2 == lo && hi2 == hi); // 独立复算与 API 冻结界一致` —— 要求两份「独立」实现**逐位相等**，这正是同源的证明。真正独立的 oracle 应允许分歧。
- 错标定证据：界的结构（`:522-534`）是 P3 专属，却被无条件套用到 P1/P2；P2 主导项是线性样本栈 `cells·tile_px·n_frames·4`（`plan_estimator.cpp:346-354`）。
- **反例已执行验证**，见 §5-C1。
- 影响：该门只被测试调用，故当前不阻断生产；但它对 P1/P2 的界是错的，且任何复用它的判据都会继承该错误。测试取样避开了失效区。

**B2｜`12u << (2*order)` 32 位回绕/UB ⇒ phase2 heavy 计划退化为 0 工作量**
- 位置：`plan_estimator.cpp:301`；守卫 `:289`；常量 `plan_estimator.h:44`（`kHipsMaxOrder = 20`）
- 生产可达性：`runtime.cpp:96` `m.input_order = u64("order")` 从节点 config 读取，`order` 完全由调用方控制
- **反例已执行验证**，见 §5-C2。
- 后果链：`cells=0` → `out_px=0`（`:306` `mul_overflow(0,·)` 不溢出）→ `work_units=0`、`max_useful_workers=0`（`:316-318`）< `min_useful_workers=1` → `:496` 的 heavy 拒绝门因 `0 != 1` 而**不触发** → `peak_memory_bytes=1 MiB`（`:353-354`）→ 经 `runtime.cpp:206` 成为内存回压的预留额。

**B3｜内存回压在节点独跑时整体失效；且失败语义与注释不符**
- 位置：`scheduler.cpp:202`（豁免条件）、`scheduler.cpp:231-235`（无条件取队首）、`runtime.cpp:212`（不可估算留 0）、`runtime.cpp:199`（该失效的原文自述）、`scheduler.cpp:360-364` + `:379-383`（SKIPPED 语义）
- 证据：任意单独运行的节点不经任何内存判定即被派发 → `memory_limit_bytes_` 对单节点从不被强制。`runtime.cpp:212` 让 `estimated_memory_bytes=0`，于是 `mem_used+0 <= limit` 恒真 —— 正是 `runtime.cpp:199` 自己点名的失效模式。
- 附带：`:379-383` 注释称 SKIPPED 是「若其依赖链在失败下游」，代码对**所有**未运行节点一律 SKIPPED，包含与失败无关的独立分支的合法下游。

### 4.2 须修（24）

| 编号 | 位置 | 问题 |
|---|---|---|
| M1 | `export_stream.cpp:612`、`:629`、`:624-625` | 伪独立复算：writer 从不累积 checksum（只算 `written`，`:432`/`:500`），`o.checksum` 全仓无消费者；读失败 `break` 无错误码，输出前缀 hash 冒充完整值 |
| M2 | `executor.cpp:19-20` vs `scheduler.cpp:373-376`、`export_stream.cpp:582-586` | 「全仓唯一池 / scheduler 不再自建池」被**同片内两处**证伪 |
| M3 | `executor.cpp:31`、`:151`、`:270` | `g_task_exceptions` 只写不读（全仓仅 3 处命中、无读取者），「唯一留痕面」不成立；注释 `:29-30` 引 detail 19_runtime §7「错误码一律上行」实际未满足 |
| M4 | `runtime.cpp:281-292`、`:321`、`:325`、`context.cpp:127-130` | `node_granted` 取自**永不复位的进程级高水位** `peak_lease`，并发节点因 `acquired_total` 全局计数互相污染 ⇒ 上一节点峰值被记到当前节点；与 `:319-320`「配置不得冒充观测」口号冲突 |
| M5 | `runtime.cpp:278`、`:328`、`context.cpp:428-433` | `call_count` 是常量 1 却以观测身份写入 trace；由此 "repeated-call" 判据（cnt>1）在生产路径**结构上不可能触发**（每节点恰一条 MODULE_CALL，调度器无重试）= 恒真门 |
| M6 | `context.cpp:410-414` vs `:366` | `detect_repeated_calls` 不按 run_id 过滤，而 `same_call_site` 含 run_id ⇒ 同一 TraceStore 跨两 run 误报 repeated-call |
| M7 | `context.cpp:466-468`、`:474-476` | NODE_START 的 RUNNING 掩盖后续 ERROR；缺 NODE_END 时失败节点被 replay 报成 RUNNING |
| M8 | `block_frame.cpp:191-206` | `consume` 的 bool 三义：成功但块存活(:205)/未声明消费者(:197)/状态非 CREATED(:195) 全 false；`if(!consume())` 判错会把成功消费报成错误 |
| M9 | `block_frame.cpp:150-161` | 五条拒绝路径全返回裸 nullptr（名字非法/重名/超 4GiB/超 16GiB/元素数溢出），无稳定错误码；`:153` 先乘后判，溢出发生在 `:154` 上限检查之前 |
| M10 | `block_frame.cpp:108-115` | 注释宣称的判据被掏空：`produced` 集合建好后从未使用，循环体只有 `(void)c;` |
| M11 | `executor.h:64` + `executor.cpp:38-39`、`:94-105`、`:200-204` | 构造函数 public，`CpuHeavyExecutor(nullptr)` 可达；此时 worker_count=1，worker 在 `:102-105` 谓词恒假处永久阻塞，且 `inflight` 已自增 ⇒ `wait_all()` 也永久阻塞。`create_cpu_heavy_executor`(:323) 的空指针拦截对直接构造无效 |
| M12 | `executor.cpp:184-191`、`:77-78`、`:200-203` vs `:280-288` | enqueue 返回 void、取消时静默丢任务（无错误码无计数），与 IoExecutor 返回 false 不对称；cancel 路径弹出全部任务后 `wait_all` 因 `cancelled` 直接返回 ⇒ **wait_all() 返回成功而 N 个任务从未执行** |
| M13 | `executor.cpp:266-271` vs `:162-172` | I/O 侧任务异常**一条 trace 都不写**（只加不可读的全局计数），CPU 侧会写 WORKER_TASK/FAILED ⇒ 失败语义不对称 |
| M14 | `executor.cpp:260` vs `:17-18` | `IoTaskClass` 存进 pair 后立即丢弃，从不读取；`:18` 宣称的标记无消费者 |
| M15 | `logging.cpp:35-38`、`:112-114`、`:34`、`:110` | JSON 无转义（`"`/反斜杠/换行 → 非法 JSONL，换行还会劈行，被 `context.cpp:445-484` 判成两条 skipped）；`buf[512]`/`buf[128]` 静默截断且无截断标记 |
| M16 | `logging.cpp:64`、`:70` vs `:74` | `Logger::log` 全程无锁而 `++seq_`/`++emitted_` 是裸自增；同文件 MetricsAggregator 却加锁 ⇒ 并发日志 seq_ 数据竞争 + 重号 |
| M17 | `context.cpp:204`、`:549`、`runtime.cpp:471`、`:481`、`executor.cpp:321`、`:331` | `noexcept` 工厂 + `make_shared`/`make_unique` ⇒ `bad_alloc` 直接 terminate；返回 `Result` 说明本意是上报失败，noexcept 把这条路堵死 |
| M18 | `cpu_budget.h:4`、`plan_estimator.cpp:7` | 悬空引用：`docs/engineering/THREADING_MODEL.md` **不存在**（`TRACEABILITY_SPEC.md:282` 显示 ENG-THREAD-001 的文档列已指向 `docs/engineering/execution_options_contract.md`，即已合并改名而引用未跟）；`lib/algorithms/coverage/coverage.cpp` **不存在**（真实路径 `lib/algorithms/coverage/src/coverage.cpp`），且原文写成畸形串「coverage coverage.cpp」 |
| M19 | `runtime.cpp:78-82`、`:205-208`、`:84-93` | `catch(...) { return m; }` 静默吞配置解析失败，无错误码、无 unestimable_reason；对 work_units 恒为 1 的分支仍标 `plan_estimated=true, plan_source="estimator"` ⇒ 配置损坏的节点在 inspect() 里显示为「已估算」；u64 把负数/错类型一律折 0 |
| M20 | `runtime.cpp:185-196` vs `:181-182` | `create()`/`plan()` 失败时既不记错也不登记，静默退化到 `(1, budget_)` 占位 —— 恰是该段注释声称要消灭的那个占位 |
| M21 | `runtime.cpp:159-161`、`pipeline.cpp:225`/`:234`、`pipeline.h:41-54` | 输入 artifact 无生产者 ⇒ 依赖边静默丢弃；`MISSING_PORT` 只校验端口名在 descriptor 中存在，**不校验 artifact 有生产者**，`IrError` 也无对应项 ⇒ 「外部文件输入」与「拼错的 artifact id」不可区分 |
| M22 | `artifact_store.cpp:18`、`:1` | 只查 `content_sha256.size()!=64`，文案却写 "must be 64 hex"，64 个 'z' 通过；文件头的「篡改检测」建立其上。对照 `phase_lifecycle.py:124` 用 `_HEX64.match` 真校验 —— 同一概念 C++ 侧更松 |
| M23 | `artifact_store.cpp:109-110`、`:91`、`:95` | `from_json` 从不调用 `validate()`；对 `coordinate`/`invalids` 做无范围检查 `static_cast`（`{"coordinate":99999}` 直落越界枚举值） |
| M24 | `plan_estimator.cpp:547-567` | `plan_estimate_to_json` 无 JSON 转义，`node_id`/`module_id`/`serial_section` 的 name/reason 原样插入；`module_id` 来自调用方 |

### 4.3 建议（35 条，摘要）

自洽/恒真门族（`phase_lifecycle.py`，均为本人重读推导）：
- S1 `:189-191` `assert_no_shared_registry` 恒 `return True`，docstring 承诺的检查从未读 `_global_seen`；且**全仓无调用者**（含测试）。
- S2 `:137-142` `read_only_external` 恒 `return True`；`eng/tests/runtime/test_phase_lifecycle.py:160` 正是在断言这个硬编码常量。
- S3 `:163-171` `assert_registry_view_isolated` 自洽：视图由 `:78` 同一谓词过滤而成，再从同一文件读回同一谓词比较 ⇒ `leaks` 结构上恒空，CLI `:224-235` 恒打印 PASS。
- S4 `:200 if p is not None` 使全未注册图 `phases=[]` ⇒ PASS（fail-open）。
- S5 `:28` `_MODULE_ID_RE`、`:30` `_MANIFEST_HEX64` 定义后从未使用；`content_digest` 用 `_HEX64`，不接受 `_MANIFEST_HEX64` 允许的 `sha256:` 前缀形式。
- S6 `:63` 每次 `phase_of_module` 重读重解析磁盘 registry，在 `:167`/`:199` 循环里退化成 O(n²) 次磁盘读。
- S7 `:241-242` `--phase-of` 对未知名也 `return 0`，脚本化调用把「未找到」当成功。

其余建议：
- S8 `block_frame.cpp:163-173`/`:216` `find` 对死块与不存在返回同一 nullptr；`destroy` erase 后调用方裸指针悬空；全类无互斥而 `create/consume/destroy` 返回裸指针。
- S9 `cpu_budget.cpp:63-64`+`cpu_budget.h:14-16` 无租约即得整机预算（**已诚实写明的刻意设计**，非隐藏缺陷，但后果需登记）；`max(1u,…)` 把 0 租约转成 1。
- S10 `logging.cpp:77` `sum_ += value` 无溢出检查，长跑可回绕致 `sum() < max()`；从不记 min。
- S11 `plan_estimator.cpp` 从不设 `e.node_id`，`:547` 恒输出 `"node_id":""`。
- S12 `plan_estimator.cpp:220` `plane_bytes*2` 无溢出守卫，而 `:522` 同类计算有守卫；与文件 `:435`「checked 拒绝，不产出饱和伪峰值」自述冲突。
- S13 `plan_estimator.cpp:127` NaN 静默走保守缺省，与 `:151`「显式拒非 TAN，无静默默认」政策不一致；`:128 (void)has_scale` 是残留 no-op。
- S14 `plan_estimator.cpp:304` 读 `in.output_pixels` 并称「优先」，但 `runtime.cpp:71-110` **从不写入该字段** ⇒ 生产路径恒 0。
- S15 `scheduler.cpp:75` 用 `std::queue` 未 include `<queue>`；`logging.cpp:74` 用 `std::lock_guard<std::mutex>` 未 include `<mutex>`（均靠传递包含）。
- S16 `runtime.cpp:455/456` 成员声明序使 `governor_` 先于 `scheduler_` 析构，而 `scheduler_` 持其裸指针（`:171`）。当前 `~Scheduler()=default` 不触碰故未爆，一旦析构用到即 UAF。
- S17 `runtime.cpp:463` `cancelled_` 被写但全类无读取者；`export_stream.cpp:577` 的 `pre_cancelled` 在 `:604` 被 `(void)` 丢弃。
- S18 `export_stream.cpp:120-123`/`:130-133` 两个同名同值 JSON 键（改名残留）；`:107`/`:137` 返回值全丢，manifest 写失败无痕。
- S19 `export_stream.cpp:684` `write_manifest()` 在失败/取消时也写，且只记配置派生量、不含 ok/error/exit_code ⇒ 不能作运行结果证据面。
- S20 `export_stream.cpp:335`/`:468` 故障注入开关 `fail_write_after_bytes` 在生产 TU（错误串含字面 `(injected)`）。**核实：生产无写入者**（仅 `eng/tests/unit/export_stream_test.cpp:144`），无生产风险，属实验开关混入生产面。
- S21 `CpuHeavyExecutor` 队列**无容量上界**，I/O 侧却有；两者 enqueue 失败语义不对称。
- S22 `context.cpp:408-413` 规则 (b) 按 node_id 计数，忽略 entry 与 module_id ⇒ 同节点合法调两个模块被误判 repeated-call。
- S23 `trace.h:81` `bool ok = true` 默认 true（任何未赋值返回路径报成功）；`trace.h:85` `error` 声明为「完全不可解析时的说明」但 `context.cpp:438-525` **从不赋值**。
- S24 `context.cpp:264-280` 字段类型错误一律折成缺省且仍 `return true`，计入 `parsed_lines` 而非 `skipped_lines` ⇒ 类型损坏行与正常行不可区分。
- S25 `context.cpp:456-476` 与 `:492-509` 行处理块逐字复制两份，改一处漏一处即静默分叉。
- S26 `plan_estimator.cpp:533` `read_bytes>>3`、`:534` `2 MiB` 是无推导经验常数；按项目硬编码处置规则应移入配置或注明出处。
- S27 `scheduler.cpp:37` `add_node` 同 node_id 静默覆盖（IR 侧 `pipeline.cpp:237` 有 `DUPLICATE_PRODUCER` 兜底，但 `add_node` 自身无保护）。
- S28 `scheduler.cpp:277-278` heavy 闸门在**已占用 active 槽与内存预留之后**才获取 ⇒ 多个 heavy 节点排队时其余 worker 全阻塞在 `heavy_mu`，无 worker 可派发独立节点（饥饿，非死锁）。
- S29 `scheduler.cpp:187`/`:191` 在持 scheduler `mtx` 下调用 `governor_->may_dispatch()`/`level()` ⇒ 隐式锁序 Scheduler::mtx → Governor::mu，当前无反向路径。
- S30 `plan_estimator.cpp:210` tiny 判据按输出像素、work_units 按行带数，两轴不同源 ⇒ 1024×16 的 heavy 会被判 tiny 而其 work_units 达 1024。
- S31 `cpu_budget.cpp:34` `sizeof(cpu_set_t)` 在 >1024 核平台使 kernel 返 EINVAL ⇒ 静默退回 `hardware_concurrency`（全机核数而非亲和核数）。
- S32 `cpu_budget.cpp:57` cap==0 静默表示「不设上限」，配置 0 的语义未在任何地方强制。
- S33 `context.cpp:149` `_note_release` 对 `g_active_leases` 无下溢保护；该全局量不参与任何判定。
- S34 `context.cpp:161` `AcquirePolicy::BEST_EFFORT` 在**生产无任何调用者**（唯一 min>1 调用是 `eng/tests/unit/rt002_budget_test.cpp:112`）；且 `:154` 强制 min≥1，故 `cur<min && cur>0` 在生产恒不成立 ⇒ BEST_EFFORT 退化为 NONBLOCK。
- S35 `scheduler.cpp:287` 节点 `fn` 为空时 `node_ok` 保持 true ⇒ **静默标 COMPLETED**。

---

## 5. 我主动构造的反例

### C1｜推翻「冻结界对 P1/P2 有效」——**推翻成功**
构造：`module_id="acsd.phase2.integrate"`、`is_heavy=true`、`n_tiles_input=C`、`n_frames=F`、`input_order≤15`，代入 `plan_estimator.cpp:281-383` 的 phase2 分支与 `:512-543` 的界公式。
期望推翻：界是由 P3 结构导出的，对 P2 的线性样本栈同样成立。

执行结果（`/tmp/g08_ce/ce.cpp`，独立自写片段，逐字照抄两处公式）：
```
8 tiles x 26 frames          C=8   F=26  peak= 209.00 MiB  max= 211.00 MiB  gate=GREEN
8 tiles x 27 frames          C=8   F=27  peak= 217.00 MiB  max= 211.00 MiB  gate=RED  <--
1 tile  x 21 frames          C=1   F=21  peak=  22.00 MiB  max=  21.12 MiB  gate=RED  <--
1 tile  x 22 frames          C=1   F=22  peak=  23.00 MiB  max=  21.12 MiB  gate=RED  <--
100 tiles x 8 frames (测试自身取样) C=100 F=8  peak= 801.00 MiB  max=11814.50 MiB  gate=GREEN
```
结论：**推翻成功**。判红条件约化为 `F ≳ C + 18`。8 tiles × 27 帧、1 tile × 21 帧都是极普通的 mosaic。而 `test_rt005_plan_estimator.py:249` 取的 100 tiles × 8 frames 落在绿区深处 —— **样本恰好避开了失效区间**（这就是「筛掉真信号」）。

### C2｜推翻「`12u << (2*order)` 在 order≤20 内安全」——**推翻成功**
构造：对 order = 13..20 求 `12u << (2*order)`。
期望推翻：守卫 `plan_estimator.cpp:289`（`input_order > kHipsMaxOrder=20` 即拒）足以覆盖该表达式。

执行结果：
```
  input_order=13 -> cells=805306368
  input_order=14 -> cells=3221225472
  input_order=15 -> cells=0        <== 静默回绕
  input_order=16 -> cells=12       <== UB（32 位类型上移位量 32）
  input_order=17 -> cells=48       <== UB
  input_order=18 -> cells=192      <== UB
  input_order=19 -> cells=768      <== UB
  input_order=20 -> cells=3072     <== UB
```
结论：**推翻成功**。`12u` 是 32 位 `unsigned int`，移位在 32 位内完成。order=15 精确回绕到 0（12·2³⁰ = 3·2³²）；order≥16 是未定义行为。守卫拦不住任何一个。

### C3｜推翻「executor.cpp:19-20『全仓唯一池、scheduler 不再自建池』」——**推翻成功**
期望推翻：该注释是文件级权威声明，应属实。
核实：`scheduler.cpp:373-376` `std::vector<std::thread> pool; ... for (i<budget_) pool.emplace_back(worker);`；`export_stream.cpp:582-586` `pool.reserve(cfg_.workers+2)` + writer/reader/compute 共 `workers+2` 条线程。
结论：**推翻成功，且两处反例都在本片内**，无需争论仓内其他子系统。

### C4｜推翻「`o.checksum` 是 writer 累积值的独立核对」——**推翻成功**
期望推翻：注释 `export_stream.cpp:612` 声称 writer 在行主序下累积 checksum，本处按同一算法独立复算以便核对。
核实：`writer_loop_raw` 只维护字节计数 `written`（`:432` 初始化、`:500` 累加），**全文件除 `:615-629` 的 FNV 循环外无任何 checksum 累积**；`grep` 确认 `ExportOutcome::checksum` 在 `lib/` 内**无任何消费者**。
结论：**推翻成功**。这是「恒绿伪装成核对」：算出一个数，不与任何东西比对，且 `:624-625` 读失败静默 `break` 无错误码。

### C5｜推翻「`may_dispatch()` 为假 ⇒ `bp_cv.wait` 不会忙等」——**未推翻，否决本条**
期望推翻：`scheduler.cpp:187` 用 `may_dispatch()` 决定入队等待，`:188-192` 的谓词却用 `level() != HIGH`，两者若不等价就会立刻返回→外层→再判→**忙等自旋**，即 `:12-14` 声称已修的 B13-R13-2 缺陷复发。
核实 `memory_pressure.cpp` 的 `may_dispatch()`：policy 关闭→true；预算不可判定→true；新采样→仅 `level==HIGH` 时 false；节流窗口→仅 `level==HIGH` 时 false。**故 `!may_dispatch() ⟺ level()==HIGH` 成立**，谓词与门条件等价，不会忙等。
结论：**我否决本条发现**（记 S29 只保留锁序观察，不报忙等）。

---

## 6. 盲复算

口径：先不看上面任何判定，遮住结论，独立对 5 条「我判定为通过/无问题」的项重新取证，看是否与既有判定一致。

| 项 | 既有判定 | 盲复算独立取证 | 一致性 |
|---|---|---|---|
| `executor_runtime.h`（39 行） | 通过（声明属实） | `shared_work_executor` 定义在 `module_adapters.cpp:303` ✓；`#include "executor.cpp"` 在 `module_adapters.cpp:257` ✓。两条声明均属实 | 一致（**偏严处**：我本已准备记为悬空，核实后否决） |
| `build_stamp.cpp`（21 行） | 通过 | `build_stamp_generated.h`（构建期生成）、`eng/tools/gen_build_stamp.py`、`docs/engineering/VERSIONING.md` 三者均存在 ✓；全文无线程池/静默降级/溢出 | 一致 |
| 12 个 kernel 名 | 无发现 | 逐个 `grep` `backend_table.inc`，`hips-bulk-transform`/`upm-spmv`/`upm-residual`/`upm-weight-update`/`rejection-statistics`/`integration-accumulate`/`calibration-pixel-transform`/`wcs-psf-batch`/`noise-snr-reductions`/`drizzle-overlap`/`drizzle-accumulate`/`drizzle-normalize` **各命中 1 次**，无悬空 | 一致 |
| `runtime.cpp:155` 重复 producer 静默覆盖 | （曾拟记为活缺陷） | `pipeline.cpp:237-240` 有 `IrError::DUPLICATE_PRODUCER` 且在 `runtime.cpp:142` 的 validate 中先行拦截 ⇒ **不是活缺陷** | 一致（**否决本条**，只保留 S27 对 `add_node` 的观察） |
| `fail_write_after_bytes` 生产风险 | 降级为建议 | 生产代码无写入者，唯一赋值在 `eng/tests/unit/export_stream_test.cpp:144`，头文件默认 0 | 一致（**主动降级**，不记阻断） |

**盲复算结论：判一致，未发现偏松或偏严。** 5 项全部与独立取证相符，其中 2 项是我在盲复算中**主动否决/降级**了自己的候选发现。

反向自查（是否有偏松）：我把 3 条定为阻断，均有执行级反例或「同片内证伪」支撑；把 `fail_write_after_bytes`、重复 producer、`may_dispatch` 忙等三条主动降级或否决，未借「看起来像问题」凑数。最重一条（B1）我特意写明它**只被测试调用、不阻断生产**，以免夸大。

---

## 7. 子代理派发记录

**派发 5 个，覆盖 4 个互斥分片（其中 1 个因我误操作重复派发同一 prompt，已如实记录）。** 派发时对每个子代理都下达了：亲自 `read` 原文、禁信任何既有审稿 markdown、零 git 写、禁编译/跑测试、禁改仓内文件、禁读 `/tmp/acsd_g08/`、每条结论给 `文件:行`、并明确「看到任何『检查通过』机制都要独立重算」。

| 子代理 | 分片 | 文件数/行数 |
|---|---|---|
| `682af1db-eb17-4cd6-b57c-9cb59a7968d6` | `export_stream.cpp` + `plan_estimator.cpp` | 2 / 1261 |
| `7b2af6e1-f623-4eea-bb48-e821baaef9e7` | **（同上，误重复派发）** | 2 / 1261 |
| `64013c86-359f-4234-bd71-9da202e671e5` | `context.cpp` + `runtime.cpp` | 2 / 1044 |
| `8cd9d758-664e-47f3-83d2-4df3a094c5ca` | `scheduler.cpp` + `executor.cpp` + `block_frame.cpp` | 3 / 1011 |
| `7c0ccf6a-09a6-4d15-8853-25a4d3d147cb` | `phase_lifecycle.py` + `artifact_store.cpp` + `logging.cpp` + `cpu_budget.cpp/.h` + `executor_runtime.h` + `build_stamp.cpp` | 7 / 924 |

**逐条复核方式（我实际执行的）**：不等待子代理结论直接采纳，而是把**每一条候选发现**都拿回原文重新取证 —— 对路径引用用 `test -e` 逐个验存在；对「零消费者/不再使用」类声明用全仓 `grep` 验活跃性；对枚举/默认值读头文件确认；对算术反例用 `/tmp` 独立片段执行。

**已否决的候选发现（4 条，全部由我自己复核后否决，未计入 §4）**：

1. ~~`ThreadLease::release` 双重归还导致 `available_` 超卖~~ —— 否决。`context.h` 中 `release()` 为 `if (size_ > 0 && release_) { release_(); } size_ = 0;`，**幂等**；`executor.cpp:175` 显式归还 + 析构归还安全。
2. ~~`scheduler.cpp:187/191` 的 `may_dispatch()`/`level()` 谓词错配导致忙等自旋~~ —— 否决，见 §5-C5，两者等价。
3. ~~`runtime.cpp:155` 重复 producer 静默覆盖是活缺陷~~ —— 否决，`pipeline.cpp:237-240` 已先行拦截。
4. ~~`executor_runtime.h` 的 `shared_work_executor` 是悬空声明 / `executor.cpp` 是死代码~~ —— 否决，定义在 `module_adapters.cpp:303`，`#include` 编入在 `module_adapters.cpp:257`，两条声明均属实。

**如实声明**：截至本报告写入时，5 个子代理的回报**尚未送达本会话**。因此 §4 的全部结论**没有一条依赖子代理输出**，均由我本人读原文 + 独立取证得出。子代理回报若后续到达，其结论一律按「他人产出只作线索」处理，须经我同样复核才可并入；与本报告冲突者以本报告为准。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线（本报告实测 HEAD，与派单文本 850a9ede 不一致）
git log -1 --format='%H %s'

# 1) 取本片成员清单与行数
awk '/INF-scheduler-002/,/^  - sid: INF-scheduler-003/' \
  "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" | head -40

# 2) 逐文件行数（合计须 = 4040）
cd lib/infrastructure/scheduler
wc -l src/export_stream.cpp src/plan_estimator.cpp src/context.cpp src/runtime.cpp \
      src/scheduler.cpp src/executor.cpp src/block_frame.cpp core/phase_lifecycle.py \
      src/artifact_store.cpp src/logging.cpp src/cpu_budget.cpp src/cpu_budget.h \
      src/executor_runtime.h src/build_stamp.cpp

# 3) B2 反例：32 位移位回绕 / UB（预期 order=15 → 0，order≥16 → UB）
mkdir -p /tmp/g08_ce && cd /tmp/g08_ce   # 见本报告随附的 ce.cpp
g++ -O0 -std=c++17 -o ce ce.cpp && ./ce

# 4) B1 反例：phase2 冻结界恒红（预期 8×27、1×21 判 RED，100×8 判 GREEN）

# 5) B2/B1 支撑常量
grep -n "kHipsTileWidthPx\|kHipsTileF32Bytes\|kHipsMaxOrder" lib/include/acsd/core/plan_estimator.h

# 6) 自洽式断言的第二来源（注意 :204-205 自承「相同公式」与 :245 的逐位相等断言）
sed -n '199,250p' eng/tests/runtime/test_rt005_plan_estimator.py

# 7) M1：checksum 无消费者 / writer 从不累积
grep -rn "checksum" lib/infrastructure/scheduler/src/export_stream.cpp
grep -rn "\.checksum" lib/ --include=*.cpp | grep -v export_stream.cpp   # 无命中

# 8) M3：g_task_exceptions 只写不读（预期仅 3 处命中、无读取者）
grep -rn "g_task_exceptions" lib/ eng/

# 9) C3：executor.cpp 的「唯一池」声明被同片两处证伪
sed -n '373,376p' lib/infrastructure/scheduler/src/scheduler.cpp
sed -n '582,586p' lib/infrastructure/scheduler/src/export_stream.cpp

# 10) M5：call_count 写死为 1
grep -n "call_count = 1" lib/infrastructure/scheduler/src/runtime.cpp

# 11) M10：block_frame 判据被掏空（预期 produced 建好后从未使用）
sed -n '108,115p' lib/infrastructure/scheduler/src/block_frame.cpp

# 12) S1/S2/S3：phase_lifecycle 恒真门与自洽式断言
sed -n '137,142p;163,171p;189,191p' lib/infrastructure/scheduler/core/phase_lifecycle.py

# 13) M18：悬空引用（预期两条 MISSING）
for p in docs/engineering/THREADING_MODEL.md lib/algorithms/coverage/coverage.cpp; do
  test -e "$p" && echo "EXISTS $p" || echo "MISSING $p"; done
grep -n "ENG-THREAD-001" docs/engineering/TRACEABILITY_SPEC.md   # 指向 execution_options_contract.md

# 14) S15：缺失 include
grep -n "^#include" lib/infrastructure/scheduler/src/scheduler.cpp    # 无 <queue>，但 :75 用 std::queue
grep -n "^#include" lib/infrastructure/scheduler/src/logging.cpp      # 无 <mutex>，但 :74 用 lock_guard<std::mutex>

# 15) §6 的 4 条否决项取证
grep -n "void release" -A 5 lib/include/acsd/core/context.h                 # 幂等
awk '/bool MemoryPressureGovernor::may_dispatch/,/^}/' \
  lib/infrastructure/scheduler/src/memory_pressure.cpp | head -25            # ⟺ level()==HIGH
grep -n "DUPLICATE_PRODUCER" lib/infrastructure/scheduler/src/pipeline.cpp  # 先行拦截
grep -n "shared_work_executor" lib/infrastructure/scheduler/src/module_adapters.cpp   # :303 有定义
grep -n 'include "executor.cpp"' lib/infrastructure/scheduler/src/module_adapters.cpp # :257

# 16) S20：故障注入开关生产无写入者
grep -rn "fail_write_after_bytes" lib/ eng/ | grep -v "src/export_stream.cpp"
```

**复跑环境说明**：§5 的两个反例在 `/tmp/g08_ce/` 的自写独立片段中执行，**不涉及仓内源码、不编译仓内 target、不运行仓内二进制**。片段逐字照抄 `plan_estimator.cpp:301` 与 `:512-543`/`:281-383` 的公式，常量取自 `lib/include/acsd/core/plan_estimator.h:42-44`（`kHipsTileWidthPx=512`、`kHipsTileF32Bytes=1048576`、`kHipsMaxOrder=20`），均为实读所得。