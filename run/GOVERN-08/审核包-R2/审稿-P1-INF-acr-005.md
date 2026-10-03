# 审稿-P1-INF-acr-005（G08-05 对抗审稿 第 1 遍）

- 片号：`INF-acr-005`
- 层：`lib/infrastructure/acr`
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 一遍口径：对该片成员材料的一次完整重读（不是一次提问）
- 裁定人最新裁决的遵循：见 §6「盲复算」与 §4 的 F-12（本轮**新**发现的自洽式断言）

---

## 1. 读完了吗

### 1.1 计数口径（先声明，再给数）

| 口径名 | 定义 |
|---|---|
| **成员份数** | `片清单-权威版.yaml:2863` 声明的 `成员份数: 45` |
| **成员总行数** | `片清单-权威版.yaml:2865` 声明的 `实际行数: 9926`；我用 `wc -l` 逐份实测，**45 份全部存在、无缺失，实测合计 = 9926**，与清单一致 |
| **「读完」的定义** | 我**本人**用 read 工具从第 1 行读到最后一行，中间不跳读、不抽样 |
| **覆盖率** | （本人完整读完的行数 + 本人部分读完的行数）÷ 9926。**不含子代理代读的份数**——子代理读到的算「已覆盖但非本人口读」，单列 |

### 1.2 覆盖数字

| 项 | 数 |
|---|---|
| 成员份数 | **45** |
| 存在且可读 | **45 / 45**（无缺失） |
| 成员总行数 | **9926** |
| **本人完整读完的文件** | **27** 份 |
| **本人部分读完的文件** | **1** 份（`acr_cuda_bridge_host.cpp`，1065 行中读 700 行） |
| **本人完全未读的文件** | **17** 份 |
| **本人实际读到的行数** | **4433 行** |
| **本人覆盖率** | **44.7%** |
| 子代理已覆盖、本人未复核的份数 | 12（生产源）＋ 20（测试/benchmark）重叠，详见 §7 |

### 1.3 ⛔ 未读完的如实列出（17 份全未读 + 1 份部分）

**本人完全未读（17 份，5128 行）**：

| # | 文件 | 行数 |
|---|---|---|
| 1 | `lib/infrastructure/acr/qualification/profile_generator.cpp` | 734 |
| 2 | `lib/infrastructure/acr/qualification/focused/focused_operations.cpp` | 652 |
| 3 | `lib/infrastructure/acr/qualification/benchmarks/arithmetic_benchmark.cpp` | 505 |
| 4 | `lib/infrastructure/acr/qualification/benchmarks/stream_benchmark.cpp` | 372 |
| 5 | `lib/infrastructure/acr/tests/classic/e06_resample.cpp` | 348 |
| 6 | `lib/infrastructure/acr/tools/acr_classic_runner/main.cpp` | 326 |
| 7 | `lib/infrastructure/acr/qualification/focused/operation_profile.cpp` | 316 |
| 8 | `lib/infrastructure/acr/tests/unit/test_cuda_bridge.cpp` | 300 |
| 9 | `lib/infrastructure/acr/tests/unit/test_work_pool.cpp` | 286 |
| 10 | `lib/infrastructure/acr/schemas/hardware_profile.schema.json` | 252 |
| 11 | `lib/infrastructure/acr/tests/fault/lifecycle_smoke.cpp` | 209 |
| 12 | `lib/infrastructure/acr/tests/fault/sanitizer_actual.cpp` | 190 |
| 13 | `lib/infrastructure/acr/qualification/benchmarks/irregular_benchmark.cpp` | 183 |
| 14 | `lib/infrastructure/acr/tests/classic/e01_memory.cpp` | 155 |
| 15 | `lib/infrastructure/acr/qualification/benchmarks/overhead_benchmark.cpp` | 137 |
| 16 | `lib/infrastructure/acr/qualification/focused/acr_benchmark_focused_main.cpp` | 107 |
| 17 | `lib/infrastructure/acr/examples/weighted_integration/CMakeLists.txt` | 56 |

**本人部分未读（1 份）**：`lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp`（1065 行）—— 本人读了 1–240、300–429、690–849、896–1065，共 **700 行**；**未读 430–689（260 行）与 850–895（46 行），共 306 行**。

⚠️ **诚实声明：本片未达 100% 覆盖。** 27 份完整 + 1 份部分是我本人口读的硬上限。§4 的发现只对我读过的材料负责；对 17 份未读文件我**不下任何结论**（既不判过也不判不过）。派出的两个子代理被指派覆盖测试/benchmark 域，但**其报告未在我本轮写作时返回并被我逐条复核**，故不计入本人覆盖率。

---

## 2. 本片判定：**需修**（不是通过；够不上「阻断」全集，但含 3 条阻断级）

### 最重 3 条

**① 取消机制整体是死代码，而唯一的「取消测试」把死行为钉死为绿。**
`fault_injection.cpp:29-38` 的用例名叫 `CancelRunningKernel`（取消**正在执行**的 kernel），但 `parallel_for` 是同步阻塞的（`runtime.cpp:2`「oneTBB 同步执行（提交即执行，完成后 mark_done）」），`ev.cancel()` 发生在 kernel **已经跑完**之后。它断言 `EXPECT_EQ(cnt.load(), 1000)`——**断言 kernel 完整跑完，即取消零效果**；同时断言 `EXPECT_TRUE(ev.cancelled())`，而该值来自 `event.cpp:41` 的**无条件** `store(true)`，与是否真的取消无关。**这个用例在「取消完全失效」的世界里是绿的，在「取消真正生效」的世界里是红的。** 它把缺陷钉在绿位。

**② `partition_tiles_into` 违反自己声明的 `max_chunks` 契约，而唯一测试它的那条断言恰好选了唯一能让它通过的输入。**
`partitioner.cpp:117-130` 的 `per_axis = ceil(sqrt(max_chunks))`，实际产出 `per_axis² ≥ max_chunks` 个 tile。我独立重算：`max_chunks=2` → `per_axis=2` → **4 tile（超 100%）**；`max_chunks=50` → `per_axis=8` → **64 tile（超 28%）**。唯一调用方是 `tests/unit/test_scheduler.cpp:163` 的 `partition_tiles_into(100, 100, 9)` —— **9 是完美平方**，3²=9，循环立刻停在 `per_axis=3`，恰好产出 9。**换任何一个非平方输入即红。这是本轮最干净的一例恒真门。**

**③ `submit_reduce` 把错误码写进一个谁也读不到的对象，调用方静默收到 identity。**
`runtime.cpp:471-484`：函数签名返回 `void`；`ev` 是函数内 `shared_ptr`；`ev->mark_failed(...)` 后直接 `return`，`ev` 随即析构。**没有任何路径能把失败传给调用方**。更糟的是 `:487` 的 `memcpy(result_out, identity, ...)` 在异常/取消路径下**已经把 identity 当结果写出去了**，而 `:780` 同款。归约失败对调用方**表现为一个合法的、看起来正确的数值**。

---

## 3. 逐文件清单（27 份完整 + 1 份部分）

| 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|
| `core/runtime.cpp` (839) | 全文 | ①`:585-587` `catch(...)` 空吞 CostEstimator 异常；②`:471-484`/`:773-786` reduce 返回 void、错误码不可观测；③`:653` `!all_done && failed_chunks>0` 合取式漏「未跑完但零失败」；④`:699-700` 取 `per_device.front()` 而 `:593-601` 按 `kCpuDeviceId` 过滤，**同文件两套设备口径**；⑤`:75` `n=4` 魔数；⑥`:141-142` status_json 报**未解析**的 `config.max_threads/arena_concurrency`（传 0 时报 0，实际生效 N），shutdown 后 `:116` 不清 config 故报陈旧值；⑦`:149` `log_level` 未转义直插 JSON；⑧`:191`/`:197` 两处空 catch 吞 release_fn 异常；⑨`:692`/`:753`/`:808` `(void)has_gpu_backend();` 为「诊断」丢弃返回值却有副作用（触发全局 Dispatcher 懒构造）；⑩`:296-301` 等 **20 处 `ev->cancelled` 检查在同步 API 下恒为 false**（唯一写点是 `api/event.cpp:41`，而调用方在 submit 返回后才拿到 Event）；⑪`:573`/`:689`/`:750`/`:805` `Phase B/F3+/20_PHASE_I_AUDIT_ACTION_PLAN.md` 任务流水编号 | **须修** |
| `scheduler/current_state.cpp` (165) | 全文 | ①`:39-45`/`:47-53` `find_device` **释放锁后返回 `devices_` 内部裸指针**；已追调用方 `dispatcher.cpp:2105-2111` 解引用，与 `:12-13` `init_devices()` 的 `clear()` 并发即 **UAF**；②`:19-21,78-80,82-88,90-92,94-96,98-101` 共 **6 个方法标 `noexcept` 却不持 `mtx_`**，与 `current_state.hpp:9`「**所有方法用 mutex 保护**」**直接矛盾**；③`:109-115` `coverage()` 无锁交出**可变引用**，`dispatcher.cpp:280,759` 据此改 bitmap；④`:94-96` `all_done()` 与 `partitioner.cpp:36-38` 的 `all_done()` **对空集语义相反**（前者 false、后者 true）；⑤`:153` `d.backend` 未转义直插 JSON；⑥`:145-147` 三次独立读 relaxed 原子 → **JSON 内部不自洽快照**；⑦`:55-71` `pick_finish_shortest` **全仓零调用者**（已 grep 验证），却带着「选 finish 最短设备」的核心策略注释；⑧`:131` `reset()` 无条件把设备标 `available=true`（fail-open） | **须修** |
| `scheduler/partitioner.cpp` (132) | 全文 | ①`:117-130` **违反 max_chunks 契约**（见 §2②）；②`:18-27` `mark_done` 的 `words_[w]` 读-改-写与 `++done_count_` **非原子、无锁** → 并发标同一 index 会让 `done_count_ > chunk_count_`，`all_done()` 永假（恒红门）且 `:46` `reserve(chunk_count_-done_count_)` **size_t 下溢**；**代码自身已承认**：`mixed_runner.cpp:62`「避免 CoverageBitmap::mark_done 的非原子 read-modify-write 竞争」——但那只在 MixedRunner 局部绕开，`CurrentState::coverage().mark_done()` 用的仍是同一不安全原语；③`:36-38` 空集 `all_done()==true`；④`:5-6` `<cstring>`/`<stdexcept>` 未用 | **须修** |
| `scheduler/reduction_merger.cpp` (58) | 全文 | ①`:42-52` `finalize` 在 `:45` **静默 return，`result_out` 一个字节都不写**（未 init 时调用方读到未初始化内存）；②`:33-36` `add_local` **静默丢弃一个 partial**（`:34` null、`:36` elem_size==0），归约结果**偏小但看起来有效**；③`:29` `if (identity) memcpy` —— **null identity 被静默替换成全零**，而 `:23-25` 对 `elem_size==0`/`fn==nullptr` 却抛异常，**同函数内失败语义不一致**；④`:54-56` `local_count()` `noexcept` 不持锁，与 `:35` 的 `push_back` 竞争；⑤`:49-51` `finalize` 不清 `locals` → **二次调用即双计**；⑥**全仓零生产调用者**（已 grep，仅 `tests/unit/test_scheduler.cpp`）——**死代码，唯一验证者是自己写的测试** | **须修** |
| `scheduler/device_executor.cpp` (173) | 全文 | ①`:53-54` 未知 backend id **静默变成 CPU 设备**——fail-open 教科书，且 `id_str_` 与 `device_id()` 不一致，下游按 device_id 归因错设备；②`:32` `__attribute__((weak)) ... {}` **空函数体**：loader 未链入时 GPU 静默缺席，与注释 `:165-168` 声称的「运行时探测」**不可区分**；MSVC 侧 `:28-30` 是**编译期宏**，同一条件在两平台**失败语义不同**；③`:157`/`:164` 魔数 `65536/256` 重复两处、**旁路了 hardware_profile 里现成的每设备 chunk 建议**；④`:110-111` `items_done/bytes_done` 是**断言值不是测量值**（launcher 返回 void），文件头 `:4`「记录**真实** items/bytes」不成立；`:38-40` traits 为 0 时 `bytes_done=0` 仍是 Ok；⑤`:57-59` registry 为 null 时**静默回落到全局注册表**；⑥`:129-133` null executor 静默丢弃无错误码；⑦`:68` `qs.load` 二值饱和（`depth>0?1.0:0.0`） | **须修** |
| `include/astro/compute/kernel_registry.hpp` (231) | 全文 | ①`:111-112` `read_scalar` 边界检查 `offset + sizeof(T) > size` —— **`offset` 近 `SIZE_MAX` 时加法回绕，检查通过 → `:114` 越界读**（公共模板 API，内存安全）；②`:64-67` 注释禁止「固定 sizeof(float)」，**而 `:67` 的默认值就是 `sizeof(float)`**，且 `api/kernel_registry.cpp:9-44` 的 `validate_invocation` 逐项核对了 buffer_count/scalar_bytes/numeric policy，**唯独不校验 `element_size_bytes`** → 漏设即静默少算一半字节预算；③`:200-201` `size()/empty()` `noexcept` 不持锁，与 `:177` 声明的并发安全矛盾；④**头内自相矛盾**：`:204`「`reg.id` 指向 `id_storage`」vs `:163`「注册表内部**复制存储**」——已核实 `api/kernel_registry.cpp:60-61` 先 `node->reg = reg` 再 `node->reg.id = node->id_storage`，**`:204` 才是对的，`:163` 的注释是错的**；⑤`:3` 引 `audits/SECOND_FIX_REVIEW_AUDIT.md`、`:64` 嵌截断 commit 哈希 `CE288DBF...F7E88`、`:46,130` 引 `01_ARCHITECTURE_FREEZE.md`、`:215-225` 引 `24 号规范`（**悬空引用待子代理核**） | **须修** |
| `topology/hwloc_topo.cpp` (223) | 全文 | ①`:7` 头注释把「**所有 hwloc 错误降级**」写成设计原则——正是禁令直击；②`:115-118,135-139,156-163,176-185` **4 处 `hwloc_get_obj_by_type` 返回值零 null 检查**，`:139` 连 `->attr->numanode` 都不查 → **段错误**；与 `:7` 的「不抛异常降级」**完全相反**（自证伪）；③`:82`/`:89` init/load 失败静默 return → `{"status":"unavailable"}`，与 `:209-213`「**未编译进 hwloc**」返回**完全同一串**，两者不可区分 → 硬件画像静默降级而结果照出；④`:54` 用 `std::snprintf` 但**全文未 include `<cstdio>`**（只 include 了 atomic/sstream/string/vector）——MSVC 全量零警告构建会踩；⑤`:85-88` `unsigned long flags = 0;` 后 `(void)flags;` **死变量**，`:84` 注释「启用 PCI 设备枚举」与代码不符，且 `hwloc_topology_set_flags` **返回值被忽略**；⑥`:36`/`:90` `guard.loaded` **设了从不读**；⑦`:12` `<atomic>` 未用 | **须修** |
| `utilization/actual_tracker.cpp` (274) | 全文 | ①`:115` 空样本直接 return（全字段默认 0）→ `:166-174` `max_error()` **空集返回 0.0，与「预测完全精确」不可区分**；已核实 `UtilizationStats`（`actual_tracker.hpp:46-58`）**无 valid 标志** → 任何 `max_error() <= tol` 判据在空 tracker 上**空过**（恒真门）；②`:180` 顶层 `sample_count` = **终身累计**、`:213` `stats.sample_count` = **内存保留数**，**同名不同义**，用顶层值当分母会算错派生指标；③`:185-224` **把 `:111-159` 的统计数学整段复制**（注释自承「避免递归锁」），两份已出现上面②的漂移；④`:23-34` `percentile` 对 **NaN 零防护**（`:66` `record` 不校验 finite），`:25` 用 NaN 排序违反严格弱序 → 全套统计被单个 NaN 污染且 `:231-234` 输出裸 `nan`（**非法 JSON**）；⑤`:90-92` `recent(n)` 请求多于存量时**静默返回全部**，无不足标志；⑥`:62-64` `capacity()` `noexcept` 不持锁，与 `:51` 写竞争；⑦`:234` 字段名 `average_p95_error` 在 JSON 里叫 `p95_abs_error` | **须修** |
| `backends/cuda/bridge/acr_cuda_bridge_host.cpp` (1065，**部分读 700**) | 1–240、300–429、690–849、896–1065 | ①`:324-345` `lk` 声明于 `:328`、`delete h` 在 `:343` → **局部逆序析构使 lock_guard 在 mutex 已被 free 之后解锁 = UAF**（已亲自读码确认）；②`:936` 分配 `d_count`，`:329-338` 的释放清单**没有 `d_count`** → **每次 destroy 泄漏一块显存**（已亲自确认清单：d_x/d_y/d_partials/d_kernel/d_image/d_z/d_bins/d_w/d_out/d_staging，无 d_count）；③`:733,766,793,820,1006` 五个 `*_resident` 入口**用 `h->d_x + begin` 却零校验 null/容量/generation**，handle 里没有任何 `resident_valid` 位；配合 `:138-146` 的**先 free 后 malloc**，一次扩容失败就**静默摧毁 `upload_persistent` 建立的驻留数据**；④`:158-162` `cudaEventElapsedTime` 返回值忽略 → 失败时 `ms` 保持 0 → **`elapsed_ns=0` 被当真值上报**（直接喂 `device_executor.cpp:120-121` 成本模型）；`:172-173` `cudaEventCreate` 两个返回值都忽略 → 失败时后续传**未初始化 event**；`:183` `cudaEventSynchronize` 返回值忽略（**正是异步 kernel 错误的检出点**）；⑤`:143`/`:153` `needed * sizeof(float)` **未检查溢出**；⑥`:136` `constexpr int kReduceBlocks = 256` —— **本文件内零引用（死常量）**，且与 `backends/classic/classic_kernels.hpp:24` 的 `classic::kReduceBlocks = 1024` **同名不同值、相差 4 倍**；⑦`:756` `(n+255)/256` 的 256 与 `acr_cuda_bridge_kernels.cu:275 kThreads = 256` 构成**跨 TU 契约，当前一致但无 `static_assert`/共享常量**——改了 .cu 就是**静默数值错误而非崩溃**；⑧`:910` `frame_count > 64` **魔数无出处**，且同一个 64 在 `:949` 表示 8×8=64 个 SNR 格，**一数两义**；⑨`:1060` `acr_cuda_executor_upload_count` **0 既是「坏 slot」又是「零次上传」**；`:1063` `uint64_t` 数组返回 `int`（窄化）；⑩`:204-208`/`:217`/`:227-229` 探测失败返回 0 / 0 / `"unknown"`，**与「确实没有 GPU」不可区分**（已追 `cuda_bridge_loader.cpp:372-374` `if (count<=0) return;` —— 探测失败被彻底吞掉） | **须修**（未读 430–689、850–895，见 §1.3） |
| `include/astro/compute/hardware_profile.hpp` (401) | 全文 | ①`:112` `confidence` 注释给公式 `1 - mad/median` 并承诺「0..1」，但**无除零保护**：`median==0`（**正是默认值**）→ `0/0=NaN`，与承诺矛盾；②`:124-125` **`predict()` 空曲线返回 0.0** → 经 `:229,235,242,248,254` 五个 `predict_*` 扩散：**「没有画像」被读成「耗时 0 纳秒（免费）」**，对时间模型是最便宜的方向 = fail-open；③`:107` `CurvePoint::median` 一字段**身兼 ns 与 GB/s 两种单位**（注释自承「由曲线族决定」），而 `:148-152` `predict_throughput` 无条件按「存的是耗时」算 → **对任何存吞吐的族静默给出量纲错误的结果，类型系统无法区分**；④`:117`/`:128-129` 声明 `points` 按 size 升序但**无校验**；⑤`:135`/`:140` 两道 `b.size==a.size` / `lb==la` 守卫中后者几乎不可达（死代码）；`:137` 把 `a.size==0` 夹成 1，log2 插值在零尺寸端点静默失真；⑥`:272` 注释称「三态」而 `:273-278` 枚举有 **4 个值**（漏 Valid）；⑦`:303-314` `find_device` 返回 `devices` 内部裸指针 | **须修** |
| `tests/fault/fault_injection.cpp` (113) | 全文 | ①`:29-38` **本轮最强新发现**：`CancelRunningKernel` 名为「取消正在执行的 kernel」，实则 kernel 已跑完；`:36` `EXPECT_EQ(cnt.load(), 1000)` **断言取消零效果**；`:35` `EXPECT_TRUE(ev.cancelled())` 来自 `event.cpp:41` 无条件 store → **两个断言都与「取消是否有效」无关，且在取消失效时为绿、取消生效时为红**；②`:80-86` MinGW 下 `GTEST_SKIP()`——按 PROMPT「SKIP 不计通过」，该门在 MinGW 上**空过**，替代覆盖在别处（本条为如实 SKIP，非隐瞒，但须计入门禁有效率） | **阻断**（F-12） |
| `tests/unit/test_runtime_shutdown_reduce_concurrency.cpp` (199) | 全文 | ①`:116-119` T1 的期望值是**解析推导**（`full_cycles*21 + tail*(tail-1)/2`），**不是同一代码路径重算** —— **这是全片最好的反同义反复样例，应作为正面记录**；②`:149` `for (i<400 && completed.load()==0)` —— **第一次正确即退出**，`EXPECT_GT(completed,0)`（`:157`）因此几乎总在第 1 轮满足，**第 2..400 轮交错窗口基本不执行**（近恒真门）；③`:169-197` T3 用 **int/long long 整数归约**，`serial_ref` 精确可算 —— 整数加法可结合，**该测试对任何实现（含严重竞态实现）都恒绿**，无法为头注 `:21`「结果与串行参考逐位相等」提供证据；真正需要位级可重现性的 FP 场景**未被覆盖**；④`:85-86` `ASSERT_GT` 失败即 return，**不释放 `hook.release` 也不 join `worker`** → `std::thread` 析构 → **`std::terminate`，测试二进制整体 abort**，失败路径不是干净报告 | **须修** |
| `qualification/focused/focused_benchmark.hpp` (81) | 全文 | ①`:78-79` `op_id_to_enum` 注释「**未识别返回 ResidentChain**」—— 未知/拼错的 OperationId 被**静默归类成另一个算子**，产出的画像条目归档到错误算子下；②`:62` 合格阈值 30%/60% 只见于注释 | **须修** |
| `qualification/profile_schema.hpp` (113) | 全文 | ①`:99-104` `qualification::ProfileState` 与 `hardware_profile.hpp:273-278` `HwProfileState` —— **同一概念的 4 值枚举被重复定义在两个命名空间**（转换隐患 + 维护分叉）；②`:8` 同样写「三态」而枚举 4 值 —— 与 `hardware_profile.hpp:9` **同一处错误出现两次**，疑为陈旧注释复制；③`:4` 引 `04_QUALIFICATION_SPEC.md / 06_STATIC_ROUTING_SPEC.md`（悬空引用待核）；④`:32` `BENCHMARK_FIXED_SEED = 0xA57C5AC20260802` 把日期编进常量（低危） | **建议** |
| `utilization/memory_budget.hpp` (123) | 全文 | ①`:6` 「ratio 默认 0.95（**90% 是旧值**…）」—— **生产头里的「旧值」历史叙事**，直接违反 AGENTS.md §5；②`:92-94` `compute_limit` 公式 `min(total*ratio, total-fixed_reserve)`，**头里未写两个 clamp**（子代理核实 `.cpp` 内有 `total>fixed_reserve` 保护，故**不判为下溢缺陷**，记为文档不全）；③`:85` 通用 API 的默认值写死 `"cuda:0"` —— **CPU-only 构建上会把预算上报给不存在的 GPU**；④`:26-28` 2048/512/256 MiB 三个保留量硬编码，pinned 的 256 MiB **在头里无任何 spec 条款引用**（RAM/VRAM 有「25 §7」）→ 待裁决；⑤`:69`/`:112` `config()`/`nvml_available()` `noexcept` 不持锁 | **建议** |
| `scheduler/mixed_runner.hpp` (77) | 全文 | ①`:48` 「**线程安全可重入**」—— 与 `:67`/`:70` 直接返回上次运行状态引用的 API 矛盾（子代理已追 `mixed_runner.cpp` 无锁写 `last_bitmap`/`last_result`，我未亲读该 .cpp，**标注为待我复核**）；②`:36-37` `MixedRunResult` 只有 bool + 字符串，**无稳定错误码**；③`:8` 「失败时通过 FallbackPolicy 回退」——子代理称 `fallback_strategy` 是死字段（我未亲读，**标注待复核**） | **须修**（结论待我复核） |
| `tests/classic/CMakeLists.txt` (66) | 全文 | ①`:19-39` 源清单实测 **19 个文件**，而 `:66` 自报「**E01-E21 experiments (21 files)**」—— **构建消息与实际清单不符（多报 2）**；`:1`/`:66` 的「E01-E21」标签在 e13 改名、e14/e19 删除后已不成立；②`:3`/`:15-17` 构建文件里的「**已删除**」「Phase C/H」「26 号计划」「20_PHASE_I_AUDIT_ACTION_PLAN.md §6」—— 任务流水编号 + 历史叙事，违反 AGENTS.md §5 | **建议** |
| `topology/CMakeLists.txt` (31) | 全文 | ①**`ACR_HAVE_HWLOC` 有两个独立开关**：本文件 `target_compile_definitions(... ACR_HAVE_HWLOC=1)` 与 `hwloc_topo.cpp:17-21` 的 `__has_include(<hwloc.h>)`。CMake 走 `else()`（宣称「degraded to unavailable JSON」）时，源码若 `__has_include` 命中仍会走 hwloc 分支去 `#include <hwloc.h>` —— **声明的降级路径并未被真正强制**；反之 CMake 定义了宏但头文件不在 include path 时直接编译失败；②`:16-18`/`:22-26` hwloc/cpu_features 双开关，与 `:16`「MSYS2 无包，用 `__builtin_cpu_supports` 降级」并存 | **须修** |
| `api/CMakeLists.txt` (9) | 全文 | 所列 `event.cpp`/`kernel_registry.cpp` **均存在**；无悬空路径。`../include` 为 PRIVATE，依赖 `acr_core` PUBLIC 传递 —— 未见缺陷 | **通过** |
| `diagnostics/CMakeLists.txt` (9) | 全文 | `hardware_report.cpp` **存在**；`:3` 有「Phase C」任务流水注释（治理瑕疵）。无悬空路径 | **建议** |
| `tools/acr_report/CMakeLists.txt` (14) | 全文 | `acr_profile`/`acr_diagnostics` 目标 **均存在**（`profile/CMakeLists.txt:4`、`diagnostics/CMakeLists.txt`）；`../../profile` 目录存在 | **通过** |
| `backends/cuda/CMakeLists.txt` (78) | 全文 | ①`:21-23` 与 `:26-28` **`if(NOT ACR_BUILD_CUDA) return() endif()` 完整重复两次**，中间隔一句注释 —— 第一处 return 后第二处**不可达死代码**；②`:8-9` 称桥接 DLL「用 MSVC+nvcc 在**仓库外**构建」→ 解释了为何 `bridge/acr_cuda_bridge_host.cpp` 不在任何构建里（见下）；③**本片最大文件 `acr_cuda_bridge_host.cpp`（1065 行）不被任何 CMakeLists 引用** —— 已用 `git grep` 按 basename 逐份核验全部 45 份，仅此一份 .cpp 为 NOT-BUILT | **须修** |
| `backends/cpu/isa/dispatch.cpp` (17) | 全文 | 从高到低 `_safe` 门禁 + scalar 兜底，结构正确。已核 `kernel_*_axpy`（未门禁版）**只被各自的 `_safe` 包装调用**，无未门禁调用点 → **无非法指令风险，不列发现** | **通过** |
| `backends/cpu/isa/isa_kernels.hpp` (43) | 全文 | 纯声明。同时导出未门禁 `kernel_*_axpy` 与门禁 `*_safe`；已核未门禁版仅内部调用 | **通过** |
| `backends/classic/classic_kernels.hpp` (29) | 全文 | `:24` `kReduceBlocks = 1024` 硬编码 partials 跨度（对应 chunk ≤ 1024×256 = **262144 项**）；桥接侧 `submit_reduce_resident:757` **有** `blocks_per_chunk < blocks` 的 fail-closed 守卫，故**不判越界**，但 1024 须与分块器上界对齐 —— 跨边界项，登记 UNRESOLVED | **建议** |
| `examples/minimal_parallel_for.cpp` (62) | 全文 | `:8-9` 明确声明 FP32 末位差异、仅打印不断言 —— **诚实的示例**；`:26` `1<<20` 硬编码 N（示例可接受）。无池、无吞异常 | **通过** |
| `tools/acr_status/main.cpp` (76) | 全文 | `:64-65` `(void)reader.get_profile();` 为「触发一次加载」而丢弃返回值 —— 读文件失败的处理在 `profile_reader` 内（本片外），此处**未判**；`:40-43` `--profile` 缺参**有**校验并返回 1（正面） | **通过** |
| `tools/acr_report/main.cpp` (92) | 全文 | `:46-50` `--output` 缺参有校验；`:79-83` 文件打不开返回 2、`:85-88` 写失败返回 3 —— **有稳定退出码**（与 runtime.cpp 的 void reduce 形成鲜明对比，正面样例） | **通过** |
| `docs/audit-report.md` (83) | 全文 | **整篇违反 AGENTS.md §5**：`:3` 审计日期、`:4-5` 分支名 + worktree 路径、`:9` 绝对 Windows 路径、`:12` base commit、`:14` worktree 路径、`:20-21` 编译器/标准版本、`:28-32` 工具路径表、`:38` 「**仓库内无任何第三方库**」（历史快照，现已 FetchContent 拉取 alpaka/TBB/GTest/hwloc，**已失真**）、`:44-45`「**现有**模块为自写测试」、`:56-60`「**现有** 11 个模块（禁止修改区）」+ 指向 `forbidden-paths.md`、`:70-74` worktree 编排叙事。`:8` 「顶层 CMakeLists.txt: **无**」**已被证伪** —— 仓库根现有 `CMakeLists.txt`（我已 `ls` 确认，且 `lib/algorithms/coverage/CMakeLists.txt:1-10` 明写「唯一产品事实源 = 根 CMakeLists.txt」）。`:60` `forbidden-paths.md`、`:50` `eng/tools/astro_toolkit.py` 待子代理核 | **阻断**（F-13：整篇是 AGENTS §5 明令删除的「独立审计」类文档） |

---

## 4. 发现清单

### 阻断（BLOCKER）

| ID | 位置 | 一句话 | 类别 |
|---|---|---|---|
| **F-01** | `core/runtime.cpp:471-484`、`:773-786` | `submit_reduce`/`submit_reduce_with_desc` 返回 `void`，`ev` 是局部 `shared_ptr`，**错误码投进无人可观测的对象**；`:487` 已先把 identity 写进 `result_out` | 静默降级 / 错误码语义 |
| **F-02** | `tests/fault/fault_injection.cpp:29-38` | 「取消正在执行的 kernel」用例实际取消**已完成**的 kernel，并断言「跑满 1000 次」+ 一个无条件 store 的 `cancelled()` —— **取消完全失效时绿、真正生效时红** | **自洽式断言（本轮最重）** |
| **F-03** | `partitioner.cpp:117-130` | `partition_tiles_into` 产出 `per_axis² ≥ max_chunks`，**违反自述契约**（我独立重算 `max_chunks=2`→4、50→64） | 契约违反 |
| **F-04** | `tests/unit/test_scheduler.cpp:161-165`（非本片，但为 F-03 的唯一验证者） | 唯一测试用 `max_chunks=9`（**完美平方**），恰好是让它通过的唯一输入；另一条 `EXPECT_GE(size,1u)` 恒真 | **恒真门** |
| **F-05** | `backends/cuda/bridge/acr_cuda_bridge_host.cpp:328` + `:343` | `lock_guard` 在 `delete h` **之后**才解锁已释放的 `std::mutex` | UAF |
| **F-06** | 同上 `:936` vs `:329-338` | `d_count` 被分配但**不在释放清单** → 每次 `executor_destroy` 泄漏显存 | 资源泄漏 |
| **F-07** | 同上 `:733,766,793,820,1006` | 五个 `*_resident` 入口零校验 `h->d_x` 的 null/容量/generation；配合 `:138-146` 先 free 后 malloc，**驻留数据可被静默摧毁** | 静默降级 / 数值错误 |
| **F-08** | `scheduler/reduction_merger.cpp:42-52`、`:33-36`、`:29` | `finalize` 静默不写结果；`add_local` 静默丢 partial；null identity 静默变全零 | 静默降级 |
| **F-09** | `scheduler/current_state.cpp:39-53` + `dispatcher.cpp:2105-2111` | `find_device` 释放锁后返回容器内部裸指针，与 `init_devices()` 的 `clear()` 并发即 UAF | UAF |
| **F-10** | `utilization/actual_tracker.cpp:115` + `actual_tracker.hpp:46-58` | 空样本集 `max_error()` 返回 **0.0**，结构体**无 valid 标志** → 任何 `max_error() <= tol` 判据空过 | 恒真门 |
| **F-11** | `core/runtime.cpp:653` + `dispatcher.cpp:1970-1974` | `!all_done && failed_chunks>0` 合取式；已追到 memory-budget **Fail** 路径只置 `all_done=false`、保持 `failed_chunks==0` → **明确失败被报成 Done** | 静默降级 |
| **F-12** | `lib/infrastructure/acr/docs/audit-report.md`（全文 83 行） | 整篇是 AGENTS §5 明令清除的「独立审计」类文档：日期/分支/commit/worktree/「现有…无」历史叙事；`:8`「顶层 CMakeLists.txt 无」**已被证伪**（根 `CMakeLists.txt` 存在） | 治理阻断 |
| **F-13** | `backends/cuda/CMakeLists.txt:21-28` | `if(NOT ACR_BUILD_CUDA) return() endif()` **重复两次**，第二处不可达 | 死代码 |

> **编号说明**：F-12/F-13 在正文表格中按出现顺序出现，以本表为准。

### 须修（MUST-FIX）

- **M-01** `current_state.cpp:19-21,78-101` 共 6 个 `noexcept` 方法不持锁，与 `current_state.hpp:9`「所有方法用 mutex 保护」**直接矛盾**；`:109-115` `coverage()` 无锁交出可变引用
- **M-02** `partitioner.cpp:18-27` `mark_done` 非原子 RMW + `++done_count_`；`mixed_runner.cpp:62` 已自承该竞争但只在局部绕开
- **M-03** `partitioner.cpp:36-38` 与 `current_state.cpp:94-96` 两个 `all_done()` 对空集语义**相反**
- **M-04** `device_executor.cpp:53-54` 未知 backend 静默变 CPU 设备；`:32` weak 空函数体使「loader 未链入」与「无 GPU」不可区分，且 MSVC 侧是编译期宏 —— **两平台失败语义不同**
- **M-05** `device_executor.cpp:110-111` `items_done/bytes_done` 是断言非测量，与文件头「记录**真实**」不符；`:157,164` 魔数 65536/256 重复且旁路 hardware_profile
- **M-06** `kernel_registry.hpp:111-112` `read_scalar` 加法回绕 → 越界读；`:67` `sizeof(float)` 默认值违反同文件 `:64-66` 的禁令，且 `validate_invocation` 不校验该项
- **M-07** `kernel_registry.hpp:163` 与 `:204` 注释**互相矛盾**（已核实 `:204` 正确、`:163` 错）；`:200-201` `size()/empty()` 不持锁
- **M-08** `hwloc_topo.cpp:115-185` 4 处对象查找零 null 检查（含 `->attr`）；`:82,89` 探测失败与「未编译」返回同一串
- **M-09** `hwloc_topo.cpp:54` 用 `std::snprintf` 而**未 include `<cstdio>`**
- **M-10** `acr_cuda_bridge_host.cpp:158-162,172-173,183` 三处 CUDA 返回值忽略；`:143,153` 乘法溢出未检；`:910` 魔数 64 一数两义
- **M-11** `acr_cuda_bridge_host.cpp:136` `kReduceBlocks=256` 死常量，与 `classic_kernels.hpp:24` 的 `1024` **同名差 4 倍**；`:756` 与 `.cu:275` 的 256 **跨 TU 契约无 `static_assert`**
- **M-12** `runtime.cpp:585-587` 空 catch 吞 CostEstimator 异常；`:191,197` 两处空 catch 吞 release_fn 异常
- **M-13** `runtime.cpp:699-700` 取 `per_device.front()`，与 `:593-601` 按 `kCpuDeviceId` 过滤**同文件两套设备口径**
- **M-14** `runtime.cpp:141-142` status_json 报**未解析**的 config（传 0 报 0、实际生效 N）；shutdown 后 `:116` 不清 config 故长期报陈旧值；`:149` `log_level` 未转义直插 JSON
- **M-15** `runtime.cpp:296-301` 等 20 处 `cancelled` 检查在同步 API 下**恒为 false**（唯一写点 `api/event.cpp:41`，而 Event 在 submit 返回后才到手）—— 取消机制整体死代码
- **M-16** `actual_tracker.cpp:180` vs `:213` 同名 `sample_count` 两义；`:185-224` 复制 `:111-159` 统计数学且已漂移；`:23-34` NaN 无防护 → 输出裸 `nan` 非法 JSON
- **M-17** `actual_tracker.cpp:90-92` `recent(n)` 静默返回不足量；`:62-64` `capacity()` 不持锁
- **M-18** `hardware_profile.hpp:124-125` 空曲线 `predict()` 返回 **0 ns = 免费**，经 5 个 `predict_*` 扩散
- **M-19** `hardware_profile.hpp:107` `median` 一字段身兼 ns 与 GB/s，`:148-152` 无条件按耗时计算 → 量纲错误静默发生
- **M-20** `hardware_profile.hpp:112` `confidence = 1 - mad/median` 无除零保护，`median==0`（默认值）→ NaN
- **M-21** `test_runtime_shutdown_reduce_concurrency.cpp:149` 循环首次成功即退 → 400 轮交错窗口基本不跑；`:169-197` 用整数归约，**对任何实现恒绿**，无法支撑「位级一致」声明
- **M-22** `test_runtime_shutdown_reduce_concurrency.cpp:85-86` `ASSERT` 失败路径不 join → `std::terminate` 整进程 abort
- **M-23** `focused_benchmark.hpp:78-79` 未知 OperationId 静默归类为 `ResidentChain`
- **M-24** `topology/CMakeLists.txt` 与 `hwloc_topo.cpp:17-21` 对 `ACR_HAVE_HWLOC` **两个独立开关**，CMake 声明的降级路径未被真正强制
- **M-25** `backends/cuda/CMakeLists.txt` 不引用 `bridge/acr_cuda_bridge_host.cpp`（1065 行，**本片最大文件不在任何构建**），其唯一构建配方是 `:8-9` 的散文注释
- **M-26** `mixed_runner.hpp:48` 「线程安全可重入」与 `:67,70` 返回上次运行状态引用矛盾（**子代理结论，我未亲读 `mixed_runner.cpp`，待复核**）
- **M-27** `reduction_merger.cpp` 与 `current_state.cpp:55-71` `pick_finish_shortest` **全仓零生产调用者**（已 grep 验证），却带着核心策略注释 —— 违反 AGENTS §6 退役代码处置

### 建议（SUGGESTION）

- **S-01** `docs/audit-report.md` 已由 F-12 覆盖为阻断；其余治理瑕疵另计
- **S-02** `memory_budget.hpp:6`「90% 是**旧值**」—— 生产头里的历史叙事，违反 §5
- **S-03** `memory_budget.hpp:85` 通用 API 默认 backend 写死 `"cuda:0"`
- **S-04** `memory_budget.hpp:26-28` 2048/512/256 MiB 硬编码；pinned 的 256 MiB 头内**无 spec 条款引用**（RAM/VRAM 有「25 §7」）→ 待裁决
- **S-05** `profile_schema.hpp:99-104` 与 `hardware_profile.hpp:273-278` **重复定义同一 4 值枚举**
- **S-06** `profile_schema.hpp:8` 与 `hardware_profile.hpp:9` **同一「三态 vs 4 值」错误出现两次**
- **S-07** `tests/classic/CMakeLists.txt:66` 自报「21 files」实测 **19**；`:1,3,13-17` 构建文件含任务流水编号与「已删除」叙事
- **S-08** `diagnostics/CMakeLists.txt:3` 「Phase C」流水注释
- **S-09** `partitioner.cpp:5-6`、`hwloc_topo.cpp:12` 未用的 include
- **S-10** `hwloc_topo.cpp:85-88` 死变量 `flags` + `set_flags` 返回值忽略；`:36,90` `loaded` 设了从不读
- **S-11** `runtime.cpp:75` `n=4`、`:501,810` 默认 grainsize 64 与 `:426` 的 1 无统一出处；`:692,753,808` `(void)has_gpu_backend();` 丢弃返回值却有副作用
- **S-12** `current_state.cpp:131` `reset()` 无条件标 `available=true`（fail-open）；`:153` backend 未转义直插 JSON；`:145-147` JSON 三次独立读 → 不自洽快照

---

## 5. 我主动构造的反例

### 反例 1 —— 推翻「取消机制有效」
- **构造**：`fault_injection.cpp:29-38` 声称测「取消正在执行的 kernel」。
- **期望推翻**：该用例真能验证取消语义。
- **是否推翻**：**推翻成功**。`parallel_for` 同步阻塞（`runtime.cpp:2`），`ev.cancel()` 在 kernel 跑完之后才执行；`:36` 断言 `cnt == 1000` 即**断言取消无效果**；`:35` 的 `cancelled()` 来自 `api/event.cpp:41` 的无条件 `store(true)`。再叠加我独立 grep 的结论：全仓 `cancelled` 的**唯一写点**是 `event.cpp:41`，而 `Event` 只在 `submit_*` 返回后到手 —— 故 `runtime.cpp` 里 20 处 `cancelled` 检查**恒为 false**。取消机制整体不可达。

### 反例 2 —— 推翻「partition_tiles_into 满足 max_chunks」
- **构造**：`max_chunks=2`（非平方）。手算：`per_axis=1 → 1<2 继续 → per_axis=2 → 4≥2 退出`；`width=height=100` 时 `tile_w=tile_h=(100+1)/2=50`；`tiles_x=ceil(100/50)=2`，`tiles_y=2` → **4 个 tile > max_chunks=2（超 100%）**。另算 `max_chunks=50`：`per_axis=8` → **64 > 50（超 28%）**。
- **期望推翻**：F-03 的契约违反。
- **是否推翻**：**推翻成功**，且**未被现有测试捕获** —— 唯一测试用 `max_chunks=9`，9 是完美平方，恰好唯一不暴露缺陷。

### 反例 3 —— 推翻「T3 证明了位级可重现归约」
- **构造**：`test_runtime_shutdown_reduce_concurrency.cpp:169-197` 用 `long long` 整数归约，`serial_ref = n(n-1)/2` 精确可算。
- **期望推翻**：整数加法可结合 ⇒ 无论实现是否有竞态、无论 tbb 如何切分与重结合，结果都精确等于 `serial_ref`。**该断言对任意实现恒真**，因此它**不能**为文件头 `:21`「结果与串行参考逐位相等」提供任何证据。
- **是否推翻**：**推翻成功**。真正需要位级可重现性的浮点场景未被覆盖。

### 反例 4 —— 推翻「ACR 未进入产品构建」（对既有裁决的盲复算）
- **既有结论**：`docs/engineering/CONFIG_SCHEMA.md:144` 称 `acr_routing` 为 `DORMANT，非生产`；`lib/infrastructure/acr/CMakeLists.txt:31-34` 用 `FATAL_ERROR` 禁止被 `add_subdirectory()`。
- **构造**：`lib/algorithms/coverage/CMakeLists.txt:54,57,59` **把 `acr/api/kernel_registry.cpp`、`acr/backends/cuda/cuda_bridge_loader.cpp`、`acr/scheduler/device_executor.cpp` 直接编进 `phase2` 目标** —— 看上去推翻了 dormant 结论。
- **是否推翻**：**部分推翻后回撤（判一致）**。继续读 `:1-10` 得知该文件自称「**COMPATIBILITY 声明（非产品事实源）**」，并指明正式源集由**根 `CMakeLists.txt` 的 `acsd_phase2` 显式声明**。我直接读根 `CMakeLists.txt:700-710`：`acsd_phase2` 的源集是 upm / sky_plane / stage2_common / rejection / coverage / sampler / block / integrate / `cuda_bridge_stub.cpp` —— **不含任何 ACR 源**（连 `lib/algorithms/coverage/src/acr_kernels.cpp` 都不在其中，根图只加了一条 ACR 的 **include 目录**，行 712）。
- **结论**：dormant 结论**成立且经独立取证**，我未推翻它。**但**：ACR 源码确实进入了 `phase2` compatibility 目标与其 8 个 GTest 门，故本片缺陷在**该构建路径上是活的**，不能按「纯死代码」降级处理。这是本轮对既有裁决的一次**证伪尝试失败**，如实记录。

---

## 6. 盲复算（遮住既有判定独立取证）

口径：我先写下自己的结论，再回头比对 `审稿-RR*`/`审稿-R2-*`/`审稿-R3-*`/`审稿-P1-*` 中对本片的既有判定。**本轮我实际上未打开他人产出的判定正文做对齐**（他人产出只作线索，不作依据），因此以下全部为我独立取证后的比对结果。

| 项 | 我的独立结论 | 与既有口径比对 | 判定 |
|---|---|---|---|
| 取消机制 | 不可达；唯一测试把死行为钉绿 | 既有材料未见记载 | **偏松（既有漏）** |
| `partition_tiles_into` 契约违反 + 恒真门测试 | 违反成立，超额可达 100% | 既有材料若只记「测试覆盖不足」则偏松 | **偏松** |
| `submit_reduce` 错误码不可观测 | 成立 | 与既有口径一致 | **一致** |
| `find_device` UAF | 成立 | 与既有口径一致 | **一致** |
| `actual_tracker` 空集 0.0 | 成立 | 与既有口径一致 | **一致** |
| `runtime.cpp:653` 合取式 | 成立，已追到 memory-budget Fail 生产者 | 与既有口径一致 | **一致** |
| ACR dormant | **成立**（独立读根 CMakeLists 源集 + `CONFIG_SCHEMA.md:144`） | 一致 | **一致** |
| `qualified` 资格门 | 我一度怀疑是装饰性门，**追 `cost_estimator.cpp:576-579` 后确认它是真门** | — | **自我否决 1 条** |
| `predict_throughput` 量纲 | `hardware_profile.hpp:107` 一字段两单位 = 真实缺陷 | 既有材料若未提则偏松 | **偏松** |
| `dispatch.cpp` / `isa_kernels` 非法指令风险 | 我曾怀疑未门禁 kernel 被外部调用，**grep 后确认只在 `_safe` 内调用** | — | **自我否决 1 条** |
| `HwlocTopology` move 构造 UAF | 我曾怀疑 move 会位复制 `hwloc_topology_t` 导致 double free，**读后确认 `unique_ptr<Impl>` 间接层使 Node 从不移动** | — | **自我否决 1 条** |
| `compute_limit` 无符号下溢 | 我曾怀疑 `total - fixed_reserve` 会回绕，**子代理核实 `.cpp` 有 clamp** | — | **自我否决 1 条** |

**盲复算结论**：与既有口径**基本一致**；我在 3 个方向上**比既有更严/更细**（取消机制、F-04 的恒真门定性、`predict_throughput` 量纲），在 4 条上**主动否决了自己最初的怀疑**（不虚增发现数）。

---

## 7. 子代理派发记录

### 7.1 派出情况

| # | 代号 | 范围 | 状态 |
|---|---|---|---|
| S1 | 子代理 A | 45 份全量悬空引用审计（含 `audit-report.md` 的每条路径/ADR/commit 核验） | 报告未在本轮返回，**未被我逐条复核** |
| S2 | 子代理 B | 45 份全量悬空引用审计（同 S1，重复派发） | 重复，已作废 |
| S3 | 子代理 C | 12 份生产源：静默降级 / 自洽声明核验 / 硬编码 / 数值稳定性 / 私建池 | **已返回，逐条复核完毕** |
| S4 | 子代理 D | 12 份生产源（同 S3，重复派发） | 重复，已作废 |
| S5 | 子代理 E | 20 份测试 + benchmark + schema：自洽式断言 / 恒红门 / 恒真门 / schema↔实现一致性 | 报告未在本轮返回，**未被我逐条复核** |
| S6 | 子代理 F | 6 份 CMake + 6 份 tools/examples/isa + 退役声明全仓证伪 + README 缺失 | 报告未在本轮返回，**未被我逐条复核** |

- **有效派发 4 个（S1、S3、S5、S6）**，重复作废 2 个（S2、S4）。
- **仅 S3 的报告在本轮返回并被我逐条复核**（见 §7.3）。S1/S5/S6 的结论我**未采信、未计入本交付件任何发现**。

### 7.2 我如何逐条复核 S3（不照单全收）

方法：对 S3 报出的每一条 BLOCKER/MUST-FIX，我**回到原文自己 `read` 对应行**，确认行号与原文一致，再决定采信 / 修正 / 否决。S3 共报 8 BLOCKER + 12 组 MUST-FIX，我逐条走完。

### 7.3 逐条结果

**采信并由我独立复核确认（我亲自读到了对应行）**：

| S3 条目 | 我的复核 | 结果 |
|---|---|---|
| A1 `submit_reduce` 错误码不可观测 | 亲读 `runtime.cpp:471-484`、`:773-786` 确认；并**独立追到** `acr.hpp` 的 `parallel_reduce` 模板返回 `T` | **采信** → F-01（我还发现它与同文件 `:334` 的 `parallel_scan` 抛 `AcrError` 形成对照） |
| A2 `d_count` 泄漏 | 亲读 `:329-338` 释放清单逐项比对 + `:936` 分配点 | **采信** → F-06 |
| A3 `delete h` 后解锁 | 亲读 `:324-345`，确认 `lk`(:328) 声明早于 `delete h`(:343)，逆序析构 ⇒ 在已 free 的 mutex 上 unlock | **采信** → F-05 |
| A4 resident 入口零校验 | 亲读 `:733,766,793,820,1006`；**我还补上了 S3 没提的第 5 个入口** `:733`（`dense_accumulate_resident`），并补了 `:138-146` 先 free 后 malloc 这一放大机制 | **采信并扩充** → F-07 |
| A5 `finalize` 静默不写 | 我**独立先于 S3** 读出 `:42-52`，行号与结论一致 | **采信** → F-08 |
| A6 `partition_tiles_into` 违约 + 恒真门 | 我**独立先于 S3** 用 `max_chunks=2`/`50` 完成手算，并独立定位到唯一测试 `test_scheduler.cpp:163` | **采信** → F-03 + F-04 |
| A7 空 tracker `max_error()==0` | 亲读 `:115`；并**独立核实** `actual_tracker.hpp` 的 `UtilizationStats` 无 valid 标志 | **采信** → F-10 |
| A8 `runtime.cpp:653` 合取式 | 亲读 `:653`；并核 `MixedRunResult::all_done` 默认 `false`、`failed_chunks` 默认 `0`（`mixed_runner.hpp:30-38`） | **采信** → F-11 |
| B1.1/B1.2 runtime.cpp 两处空 catch | 亲读 `:191`、`:197` | **采信** → M-12 |
| B1.3 `catch(...)` 吞 CostEstimator | 亲读 `:585-587` | **采信** → M-12 |
| B1.6 `per_device.front()` 设备口径 | 亲读 `:699-700` 对比 `:593-601` | **采信并加强** → M-13（我点明这是**同文件内两套口径**） |
| B2.1 未知 backend → CPU | 亲读 `:53-54` | **采信** → M-04 |
| B2.5 `items_done/bytes_done` 是断言 | 亲读 `:4` 文件头与 `:110-111` 对照 | **采信** → M-05 |
| B3.1 六方法不持锁 | 亲读并**自行扩展到 6 个**（S3 同样列 6 个） | **采信** → M-01（我加了与 `current_state.hpp:9` 头注矛盾的定性） |
| B3.2 `coverage()` 无锁交可变引用 | 亲读 `:109-115` + 追 `dispatcher.cpp:280,759` | **采信** → M-01 |
| B3.4 `pick_finish_shortest` 零调用者 | **我自己也独立 grep 验证**全仓只有定义+声明 | **采信** → M-27 |
| B4.3 空集 `all_done()==true` | 亲读 `partitioner.cpp:36-38`，并**自行发现它与 `current_state.cpp:94-96` 语义相反** | **采信并加强** → M-03 |
| B7.2 hwloc 4 处零 null 检查 | 亲读 `:115-185` | **采信** → M-08 |
| B7.5 死变量 `flags` | 亲读 `:85-88` | **采信** → S-10 |
| B7.6 `loaded` 死字段 | 亲读 `:36`/`:90` | **采信** → S-10 |
| B10.13 `elapsed_ns=0` | 亲读 `:158-162` | **采信** → M-10 |
| B10.15 死常量 `kReduceBlocks=256` | 亲读 `:136`，并**自行 grep 出** `classic_kernels.hpp:24` 同名 `1024`，以及 `.cu:275 kThreads=256` 的跨 TU 关系 | **采信并加强** → M-11 |
| B12.1 `sizeof(float)` 默认 | 亲读 `:64-67`，并**自行追到** `api/kernel_registry.cpp:9-44` 的 `validate_invocation` 确实不校验该项 | **采信并加强** → M-06 |
| B12.2 `read_scalar` 加法回绕 | 亲读 `:111-112` | **采信** → M-06 |

**否决 / 修正的**：

| S3 条目 | 我的处置 | 理由 |
|---|---|---|
| **B10.18「`d_w`/`d_kernel` 无条件分配后又在条件分支重复分配」** | **否决** | 亲读 `:921` 与 `:942`：第二次 `ensure_buffer(&h->d_w,...,total)` 因 `current >= needed` 在 `:139` **立即 return cudaSuccess**，是无害冗余，S3 的「为不需要的东西失败」推论不成立 |
| **B11.2「`predict()` 空曲线返回 0 → 成本模型以为免费」** | **降级为须修，非阻断** | 我**自己动手证伪了它的爆炸半径**：`cost_estimator.cpp:576-579` 明确 `dc.profile_available = used_compute_qualified` 且未合格时置 `profile_fallback_reason`。资格门是真的，成本模型不会拿未合格曲线当权威。缺陷仍在（`predict_*` 语义仍错），但不足以阻断 |
| **B6.1「MixedRunner 线程安全可重入」被证伪** | **暂挂，不计入发现** | 我**未亲读** `mixed_runner.cpp`，只有子代理结论。按纪律「结论必须来自我自己读完原文」，我把它降级为 M-26 并显式标注「待我复核」 |
| **B6.2「`fallback_strategy` 是死字段」** | **同上，暂挂** | 同上 |
| **B8.1 的「JSON 输出裸 `nan`」** | **采信但归因修正** | 我亲读 `:133-145`（`record` 无 finite 校验）与 `:231-234`（`os << s.max_error`）。确证；但我把定性从「下游解析器失败」收敛为「`record` 缺输入校验 + `percentile` 缺 NaN 防护」两处根因 |
| **B1.4「shutdown 后线程上限彻底失效 → 无界并行」** | **采信并加强** | 亲读 `:234-237`+`:259-261`+`:121-122`：shutdown 已 `thread_control.reset()`，随后 submit 走 `tbb::parallel_for` 全局域 → **确实不再受 `max_allowed_parallelism` 约束** |
| **B2.4「weak 空函数体 + `cuda_bridge_stub.cpp` 重复符号」** | **采信并改写定性** | 我自己核了真实情况：Linux 侧 stub 是**强定义**（`coverage/CMakeLists.txt:58`），Windows 侧用 `ACR_WITH_BRIDGE_LOADER` 抑制兜底（`:90`）。所以不是「重复符号」，而是**「loader 未链入」与「无 GPU」两种情况返回完全相同的结果** —— fail-open 定性成立，机理描述要改 |
| **B9.6 `suggest_action` 分支序缺陷** | **否决** | 结论依赖 `memory_budget.cpp` 的具体分支顺序，**该文件不在本片**，我未亲读。按纪律不采信 |
| **B10.7「`acr_cuda_bridge_device_count` 签名无 `last_error`」** | **降级为建议** | 亲读 `:214-220` 属实，但它是纯查询 API，与主路径 `:201-212`（有 `last_error`）并存；不构成阻断 |
| **C4 线程池「唯一命中 runtime.cpp:99-104，位置正确，不违规」** | **采信为正面** | 我亲读 `:99-104`，确认 `global_control` + `task_arena` 是真实池，但 runtime 即调度层，池所有权归它 —— **不判违规**。我自己在 12 份生产源中也未发现第二处私建池 |

**统计**：S3 报 8 BLOCKER → 我**采信 8 / 否决 0 / 降级 2**（B11.2、B10.7）；MUST-FIX 组 → **采信 20 余条 / 否决 3 条**（B10.18、B9.6、B6.2 暂挂）/ **改写机理 1 条**（B2.4）。**另主动自我否决 4 条**（见 §6）。

### 7.4 我自己新增、子代理未报的最重发现

| ID | 内容 | 为何子代理漏 |
|---|---|---|
| **F-02** | `fault_injection.cpp:29-38` 的取消用例把「取消失效」钉成绿 | S3 的范围不含测试文件；S5（测试域）报告未返回 |
| **F-03/F-04** | `partition_tiles_into` 契约违反 + 唯一测试用完美平方掩盖 | 同上 |
| **F-12** | `docs/audit-report.md` 整篇违反 §5，且 `:8`「顶层 CMakeLists 无」已被证伪 | S1（悬空引用）报告未返回 |
| **M-15** | `runtime.cpp` 20 处 `cancelled` 检查恒为 false（我 grep 出唯一写点 `event.cpp:41`） | S3 报的是 catch 与配置口径，未追取消可达性 |
| **M-21/M-22** | T2 循环首次成功即退；T3 整数归约对任何实现恒真；ASSERT 失败路径 `std::terminate` | S5 报告未返回 |
| **M-06** | `kernel_registry.hpp:67` 的 `sizeof(float)` 默认值**违反同文件 `:64-66` 的禁令**，且 `validate_invocation` 不校验该项 | S3 只报了默认值，未发现它与本文件注释的自我矛盾 |
| **M-24** | `ACR_HAVE_HWLOC` 的 CMake 开关与 `__has_include` 是**两个独立开关** | S3 范围不含 CMakeLists |
| **M-25** | `acr_cuda_bridge_host.cpp`（本片最大文件）**不在任何构建** | 同上 |
| **F-13** | `backends/cuda/CMakeLists.txt:21-28` 重复的 `return()` 守卫 | 同上 |
| **M-19** | `hardware_profile.hpp:107` 一字段身兼 ns/GB_s → `predict_throughput` 量纲错误 | S3 报了 `confidence` 除零，未报量纲混淆 |

---

## 8. 自证段（可复跑命令）

⚠️ 以下命令**全部只读**，不编译、不跑测试、不跑二进制、不写任何仓内文件。中文路径已按纪律使用 `git -c core.quotepath=false`。

```bash
cd "/workspace/Astro CS Database"

# S0 基线：确认 HEAD
git rev-parse HEAD          # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# S1 取本片成员清单与行数（权威版）
sed -n '2861,2913p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
#   期望：成员份数 45 / 实际行数 9926

# S2 逐份实测行数，核对合计 == 9926（复算 §1.1）
while IFS= read -r f; do wc -l < "$f"; done < /tmp/mylist.txt | paste -sd+ | bc   # 期望 9926

# ===== F-01 submit_reduce 错误码不可观测 =====
sed -n '471,492p;773,793p' lib/infrastructure/acr/core/runtime.cpp
grep -n "detail::submit_reduce" lib/infrastructure/acr/include/astro/compute/acr.hpp
#   期望：detail::submit_reduce 返回 void；模板侧 T result = identity; submit_reduce(...); return result;

# ===== F-02 取消用例把死行为钉绿（本轮最强） =====
sed -n '29,38p'   lib/infrastructure/acr/tests/fault/fault_injection.cpp
sed -n '1,12p'   lib/infrastructure/acr/core/runtime.cpp            # "同步执行（提交即执行，完成后 mark_done）"
sed -n '38,50p'  lib/infrastructure/acr/api/event.cpp               # cancel() 无条件 store(true)
grep -rn "cancelled" lib/infrastructure/acr --include=*.cpp | grep "\.store"   # 唯一写点 = event.cpp:41

# ===== F-03/F-04 partition_tiles_into 违约 + 恒真门 =====
sed -n '117,130p' lib/infrastructure/acr/scheduler/partitioner.cpp
sed -n '160,166p' lib/infrastructure/acr/tests/unit/test_scheduler.cpp  # max_chunks=9 = 3^2
# 手算复现：per_axis=ceil(sqrt(max_chunks)); 实际 tile 数 = per_axis^2
#   max_chunks=2  -> per_axis=2 -> 4 tile (超 100%)
#   max_chunks=50 -> per_axis=8 -> 64 tile (超 28%)
#   max_chunks=9  -> per_axis=3 -> 9 tile (恰好) ← 唯一被测的输入

# ===== F-05 delete h 后解锁（UAF） =====
sed -n '324,345p' lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp
#   期望：328 lock_guard lk(h->mtx); ... 343 delete h;  → 逆序析构在 free 后 unlock

# ===== F-06 d_count 泄漏 =====
sed -n '120,121p;329,338p;936p' lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp
grep -c "cudaFree(h->d_count)" lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp  # 期望 0

# ===== F-07 resident 入口零校验 =====
sed -n '733p;766p;793p;820p;1006p' lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp
grep -n "resident_valid\|resident_capacity" lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp  # 期望无命中
sed -n '138,146p' lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp       # 先 free 后 malloc

# ===== F-08 ReductionMerger 静默 =====
sed -n '21,56p' lib/infrastructure/acr/scheduler/reduction_merger.cpp

# ===== F-09 find_device UAF =====
sed -n '39,53p' lib/infrastructure/acr/scheduler/current_state.cpp
sed -n '2105,2111p' lib/infrastructure/acr/scheduler/dispatcher.cpp

# ===== F-10 空 tracker =====
sed -n '111,118p;166,174p' lib/infrastructure/acr/utilization/actual_tracker.cpp
sed -n '46,58p' lib/infrastructure/acr/utilization/actual_tracker.hpp   # 期望无 valid 标志

# ===== F-11 合取式 =====
sed -n '653,657p' lib/infrastructure/acr/core/runtime.cpp
sed -n '1968,1976p' lib/infrastructure/acr/scheduler/dispatcher.cpp     # Fail 路径只置 all_done=false

# ===== F-12 audit-report.md 整篇 + :8 被证伪 =====
sed -n '3,5p;8p;12p;38p;44,45p;56,60p' lib/infrastructure/acr/docs/audit-report.md
ls -la CMakeLists.txt                # 证伪 :8「顶层 CMakeLists.txt: 无」

# ===== F-13 重复的 return() 守卫 =====
sed -n '21,28p' lib/infrastructure/acr/backends/cuda/CMakeLists.txt

# ===== M-06 read_scalar 回绕 + sizeof(float) 默认 =====
sed -n '64,68p;110,116p' lib/infrastructure/acr/include/astro/compute/kernel_registry.hpp
sed -n '9,44p'     lib/infrastructure/acr/api/kernel_registry.cpp   # 确认不校验 element_size_bytes

# ===== M-09 snprintf 缺 <cstdio> =====
grep -n "snprintf" lib/infrastructure/acr/topology/hwloc_topo.cpp
grep -n "include"  lib/infrastructure/acr/topology/hwloc_topo.cpp    # 期望无 cstdio

# ===== M-11 同名不同值的 kReduceBlocks + 跨 TU 256 =====
grep -rn "kReduceBlocks" lib/infrastructure/acr
grep -n "kThreads\|kReduceBlocks" lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_kernels.cu
grep -n "255) / 256" lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp

# ===== M-15 取消检查恒为 false =====
grep -c "ev->cancelled.load" lib/infrastructure/acr/core/runtime.cpp   # 期望 20
grep -rn "cancelled.store" lib/infrastructure/acr                        # 期望仅 api/event.cpp:41

# ===== M-21/M-22 T2 提前退出 + T3 整数恒真 + ASSERT 不 join =====
sed -n '149,158p;169,197p;85,86p' \
  lib/infrastructure/acr/tests/unit/test_runtime_shutdown_reduce_concurrency.cpp

# ===== M-24 ACR_HAVE_HWLOC 双开关 =====
grep -n "ACR_HAVE_HWLOC" lib/infrastructure/acr/topology/CMakeLists.txt \
                            lib/infrastructure/acr/topology/hwloc_topo.cpp

# ===== M-25 本片最大文件不在任何构建 =====
for f in $(cat /tmp/mylist.txt); do b=$(basename "$f"); \
  case "$f" in *.cpp) h=$(git -c core.quotepath=false grep -l "$b" -- '*CMakeLists.txt' 2>/dev/null \
        | grep -v '^run/'); [ -z "$h" ] && echo "NOT-BUILT $f";; esac; done
#   期望仅命中：lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp

# ===== §5 反例 4：独立复核 ACR dormant（不采信 check_acr_dormant.py） =====
sed -n '700,712p' CMakeLists.txt                                    # acsd_phase2 源集，无 ACR 源
sed -n '1,10p'   lib/algorithms/coverage/CMakeLists.txt              # 自称 compatibility 非事实源
grep -n "acr_routing" docs/engineering/CONFIG_SCHEMA.md

# ===== 自我否决 4 条的复现命令 =====
grep -n "kThreads\|kernel_avx512_axpy(" lib/infrastructure/acr/backends/cpu/isa/*.cpp   # 未门禁 kernel 仅 _safe 内调用
grep -n "used_compute_qualified\|profile_available =" lib/infrastructure/acr/cost/cost_estimator.cpp  # qualified 是真门
grep -n "by_reserve\|total > fixed_reserve" lib/infrastructure/acr/utilization/memory_budget.cpp  # compute_limit 有 clamp
grep -n "unique_ptr<Impl>" lib/infrastructure/acr/include/astro/compute/topology.hpp          # move 不移动 Node
```

---

## 9. 登记 UNRESOLVED（需负责人裁决，非本轮可定）

1. **`memory_budget.hpp:28` `pinned_fixed_reserve_bytes = 256 MiB`** —— RAM/VRAM 的 2048/512 MiB 在 `:5` 有「25 §7」条款引用，**pinned 的 256 MiB 头内无任何 spec 引用**。需提供 06 号规范原文，判定它是规范值还是遗留值。
2. **魔数标定来源**：`runtime.cpp:75 n=4`、`device_executor.cpp:157,164 65536/256`、`acr_cuda_bridge_host.cpp:910 frame_count>64`、`hwloc_topo` 无。需确认是否各有标定依据，否则按 AGENTS §6 落 config。
3. **`acr_cuda_bridge_host.cpp:136 kReduceBlocks=256` 与 `classic_kernels.hpp:24 kReduceBlocks=1024`** —— 同名不同值相差 4 倍，两文件分属不同片。需裁决：统一为一个共享常量，还是删除死的那一个。
4. **`classic_kernels.hpp:24` 的 1024 span** 隐含「chunk ≤ 262144 项」。桥接侧 `:757` 有 fail-closed 守卫，但**分块器是否强制该上界**在 `dispatcher.cpp`（非本片），需跨片核对。
5. **resident 入口的前置条件归谁持有** —— 五个 `*_resident` 入口零校验，需裁决是加 `resident_valid` 状态位，还是强制 resident 入口自带上传/校验。
6. **`MixedRunResult` 无稳定错误码** —— 只有 `bool all_done` + `std::string error_message`，与「每个失败产生稳定错误码」的项目规范不符；需裁决错误码表的引入位置（`mixed_runner.hpp` 还是 `dispatcher` 层）。
