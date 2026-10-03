# 审稿-P1-INF-acr-001 — G08-05 对抗审稿 第 1 遍

- **片号**：INF-acr-001
- **层**：`lib/infrastructure/acr`
- **基线 HEAD**：`850a9edefd47434b9ab71bc907c3de1e0814b323`（提交「修正样条系数缺 h 因子，并撤回恒真门登记表的全域分母声称」）
- **日期口径**：本轮交付，零 git 写、零编译、零测试执行、未改任何仓内文件、未读 `/tmp/acsd_g08/`

---

## 1. 读完了吗

**成员份数 / 读了几分 / 成员总行数 / 实际读了多少行 / 覆盖率**

| 项 | 值 |
|---|---|
| 权威清单成员份数 | **42** |
| 实际 `read` 原文份数 | **42** |
| 成员总行数（权威清单 `实际行数`） | **9920** |
| 实际读了多少行 | **9920** |
| 覆盖率 | **42/42 份 = 100%；9920/9920 行 = 100%** |
| 成员文件缺失 | **0**（42 个成员在 HEAD 全部存在） |
| 未读完的成员 | **无** |

**计数口径（必须写明）**：行数取 `git show HEAD:<path> | wc -l`，即**按换行符计数、含空行、含纯注释行、不含无换行的末行**。我用该口径逐文件求和得 **9920**，与权威清单 `片清单-权威版.yaml:2661 实际行数: 9920` **精确一致**，可作为覆盖率未被稀释的独立佐证。42 份全部以换行结尾，故 `wc -l` = 全文实际行数。

**为核验本片结论而额外只读的片外文件（只读，未改）**：`docs/ADR-002-oneTBB.md`、`docs/ADR-009-cpu-only-build-gate.md`、`tests/sanitizer/CMakeLists.txt`、`tests/CMakeLists.txt`、`backends/cuda/cuda_backend.hpp`、`core/CMakeLists.txt`、`core/task_descriptor.cpp`、`examples/weighted_integration/route_profile_calibration.{hpp,cpp}`、`CMakePresets.json`、`VERSION`。这些只用于验证本片内的断言是否成立，**未把它们的结论算作本片覆盖**。

---

## 2. 本片判定

### **判定：阻断**

ACR 是 dormant 实验树（`CMakeLists.txt:3-41` 自述 ACR-001 休眠约束），本应低风险。但本片同时命中**私建线程池、静默降级、恒真门、自洽式断言、悬空引用**五类，且关键判据在**结构上不可能失败**。更重要的是：整条证据链（sanitizer → evidence manifest）**在什么都没跑时是绿的**。

### 最重 3 条

**【B1｜阻断】`cuda_backend.cu:102-103,172` — `std::call_once` 丢弃失败状态，第一跑红、之后永远报成功。**
`StatusCode result = StatusCode::Ok;` 是函数内局部变量（`:102`），被 `std::call_once` 的 lambda 按引用捕获（`:103`）。首次调用 lambda 执行、写入真实错误码；**此后每次调用 lambda 都不再执行，`result` 保持 `:102` 的 `Ok` 并被返回**。头文件 `cuda_backend.hpp:47-49` 明确承诺「返回 Ok / DeviceLost / OutOfMemory / KernelFailed」「幂等：多次调用安全」——**幂等做到了，状态语义没做到**。叠加 `:104 initialized_ = true` 置于全部可失败步骤之前，任何只看 `initialized()` 的消费者都会认为后端已就绪。

**【B2｜阻断】`run_acr_sanitizers.ps1:192` — 整个 sanitizer 证据链在「什么都没跑」时 exit 0 并打印 "all recorded"。**
`$failed = $results | Where-Object { $_.status -eq "FAIL" -or $_.status -eq "TIMEOUT" }` 只收两种状态；`SKIPPED`（`:55`、`:146`、`:173`）与 `FAILED_TO_START`（`:101`）**都不在失败集内**。更隐蔽的是 `:128`/`:157` 的 `if (Test-Path $chunkExe/$bridgeExe)` 为假时**连一条结果都不添加**——不是记 SKIPPED，是从汇总里静默消失。而本文件头 `:10-11` 白纸黑字写着「工具缺失或未运行项记录为 SKIPPED，**绝不冒充通过**」。**代码做的恰是注释禁止的事。**

**【B3｜阻断】`dispatcher.cpp:1671-1713` — MemoryBudget 的永久 `Fail` 动作被兜底清尾静默推翻。**
`Fail` → `gate.close_permanent()`（`:1259-1264`）→ 所有 worker `break`（`:1282`）。但 join 之后，`:1671` 的「工作保持」兜底循环会把**剩余全部工作照常执行完**：它跳过 `:1394` 整个内存预算检查块、跳过 `:1536-1562` 整个 pinned staging 记账，并在 `:1678` 用 `claim_next_dynamic(exec->id(), rem)` 一次领走**全部剩余**——与 ShrinkBlock 追求的缩块方向完全相反。同一函数 `:1720` 还把 `gate_aborted = true` 记成「已放弃」。**记账说失败，执行说成功，二者不可能同时为真。**

---

## 3. 逐文件清单（42 份，全部读完）

格式：`读了什么 → 看到什么 → 判定`。行号均为 HEAD 850a9ede。

| # | 文件 | 行数 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|---|
| 1 | `scheduler/dispatcher.cpp` | 2640 | 全文 3 段读完 | `:1181-1186`+`:1665` 裸 `std::thread` 池（`:1158` `std::barrier`），规模取 `:1140 hardware_concurrency()`，而同文件 `:357`/`:477` 走 `parallel_batch`；`:2390-2397`/`:2435-2442` 调 `legacy_parallel` 后**无条件**写 `all_done=true/done_blocks=1/items_done=n`；`:1671-1713` 兜底清尾绕过 `:1394` 内存检查与 `:1536` staging 记账；`:258`/`:269`/`:1857`/`:2165`/`:2285`/`:2378` 设备身份写死 `"cuda:0"`；`:2317` `vram_available_bytes==0` 解释为「无限制」；`:2322-2334` BdrCacheKey 不含 RAM/VRAM 余量；`:2605` `coverage.failed=0`；`:328` `read_bytes/12+1`、`:332`/`:334` `256u*1024`、`:1134` `*0.35` 等无出处魔数 | **阻断** |
| 2 | `profile/profile_reader.cpp` | 610 | 全文 | `:102-103`/`:172` `result` 按引用捕获进 `call_once`，**首次失败后每次调用返回 `Ok`**；`:104` `initialized_=true` 在所有失败检查之前；`:146`/`:152` `cudaDriverGetVersion`/`cudaMemGetInfo` 返回值丢弃 → 报 `"driver_version":"0.0"` / `"free_memory":0`；`:176` 无设备时 `sync()` 返回 `Ok`；`:95-112` double→整型无范围检查（`-1`/`1e999` 为 UB）；`:589-596` `status_json` 不转义；`:552-562` `profile_state()`/`profile_path()` 无锁读非原子成员；`:419` CWD 相对路径；`:383` 兜底 4 核、`:390-410` 11 个开销常数写死 | **阻断** |
| 3 | `utilization/system_metrics.cpp` | 532 | 全文 | `:351-356` NVML 读失败写 `valid=true`（对照 `:420-422` 同一文件相反做法）；`:179-186`/`:200-204` 枚举失败/零句柄仍 `nvml_loaded=true` → 对外报 healthy；`:183-186` 注释称「保留 dll 以便后续重试」但 `:130` 后 `nvml_init_attempted` 永不复位，**重试不可能发生**；`:445-451`/`:480-482` 无锁读 `nvml_devices`/`backends`；`:473-478` `noexcept` + `lock_guard` → 锁失败即 `terminate`；`:210-219` `backend_to_index` 死代码 + `catch(...) {return 0;}` | **须修** |
| 4 | `utilization/system_metrics.hpp` | 117 | 全文 | `:12`「无 GPU 时明确标记 estimated=true，**不伪报**」、`:13`「线程安全（多 worker 同时读取）」、`:59`「内部用 mutex 保护」——三条均被 .cpp 违反（见上） | **须修**（声明与实现相悖） |
| 5 | `utilization/memory_budget.cpp` | 237 | 全文 | `:107` `pinned_used = std::min(used_ram, pinned_limit)` → **恒不超过限值，任何 `pinned_used>limit` 门永假**；`sample()` 从不设 `pinned_exceeded`；`:69` `limit==0 → Fail`，而 `:58` `total==0→0`，故 **一次 RAM 采样失败 = 全量 Fail 且不可恢复**，与 `:604-605` 的守卫路径不对称；`:73-82` 0.05/0.15/0.30/0.50 四档阈值写死；`:38-40`/`:102-105` 无锁读 `cfg`；`:130-133` 多 GPU 时 `last_total_vram` 只存最后一个；`:175`/`:186`/`:209` 注入值标成 `valid=true` | **阻断** |
| 6 | `backends/cuda/cuda_backend.cu` | 207 | 全文 | 见 **B1**；另 `:120` `cudaSetDevice(0)` 写死单卡；`:31` `count*sizeof(T)`（在 .hpp） | **阻断** |
| 7 | `backends/cuda/cuda_backend.hpp` | 125 | 全文（片外只读，为验证 B1 的契约） | `:47-49` 明确写「返回 Ok / DeviceLost / OutOfMemory / KernelFailed」「幂等」；`:115` `static_cast<unsigned int>(grid)` 无溢出检查 | 契约被 .cu 违反 → **阻断**（证据） |
| 8 | `backends/cuda/cuda_buffer.hpp` | 164 | 全文 | `:29-37` 分配失败把 `count_` 归零，**容量信息被销毁**，后续 `copy_h2d` 报 `OutOfBounds` 而非 `OutOfMemory`；`:31` `count*sizeof(T)` 溢出无守卫；`:147-151` `cudaEventElapsedTime` 返回值丢弃 → 失败时返回 **0.0 ms**（性能最好值）；`:117`/`:141`/`:144` 三处 CUDA 调用返回值全丢；`:77`/`:94` `n==0` 先于空指针检查返回 `Ok` | **须修** |
| 9 | `backends/cpu/isa/avx512.cpp` | 49 | 全文 | `:38-47` `kernel_avx512_axpy_safe` 只验 `caps.has_isa(...)`，**从不验 `ACR_X86_AVX512` 是否定义**——MSVC 下 `:32-34` 走标量却仍 `return true`；`:24`(FMA) 与 `:27`(标量尾) 舍入次数不同 → 结果随 `n mod 16` 变；`:21` `i+16<=n` 溢出模式；`:41-43` 子集独立校验（**符合 ADR-004:21/32，此项通过**） | **须修** |
| 10 | `backends/cpu/isa/sse.cpp` | 44 | 全文 | `:37-42` 同 `:9` 缺陷；`:23`(mul+add) 与 `:26`(标量尾，可能被 `-ffp-contract` 收缩) 不一致；`:20` `i+4<=n` 溢出模式 | **须修** |
| 11 | `include/astro/compute/runtime_internal.h` | 57 | 全文 | `:36` `mark_cancelled()` 不写 `error_code` → **被取消的任务对错误码读取者呈现 `Ok`**；`:44-50` `wait()` 无超时 → 任务停在 `Pending` 即永久挂起；`:23-24` `error_code`/`error_msg` 非原子，`:51` `ready()` 无锁返回不构成对其的 happens-before；`:22` `cancelled` 死成员；`:34` vs `:42` notify 时机不一致 | **须修** |
| 12 | `core/task_descriptor.hpp` | 136 | 全文 | `:104-112` `make_reduce_descriptor` 与 `:73-81` `make_range_descriptor` **逐字节相同**，不设任何 reduction 语义——而 `dispatcher.cpp:329-334` 依 `traits.task_class==reduction` 决定是否计 256KiB partial/merge，漏设即**低报峰值**；`:131`/`:134` 声明的实现在 `core/task_descriptor.cpp`（**已核实存在，非悬空**） | **须修** |
| 13 | `scheduler/partitioner.hpp` | 84 | 全文 | 纯声明，实现在他片。`:46` `mark_done` 为 `noexcept` 而 `:63` `done_count_` 是与位图并行的缓存计数——重复 mark 的语义无法在本片判定 | UNRESOLVED（需 `partitioner.cpp`，属 INF-acr-002） |
| 14 | `scheduler/residency_manager.hpp` | 124 | 全文 | `:43` `std::string device_id{"cuda:0"}` 默认写死；结构上每 buffer 只有**单个** `device_id` 与**单个** `ResidencyState`，**无法表达多 GPU 同时驻留** | **须修** |
| 15 | `qualification/focused/operation_profile.hpp` | 112 | 全文 | `:11` 指向 `schemas/operation_profile.schema.json` — **该文件不存在**（schemas/ 下仅 5 个文件，已实测），`schema_version{"acr-operation-profile-1"}` 无外部正本。默认值偏保守（fail-closed 方向正确） | **须修**（悬空引用） |
| 16 | `qualification/focused/focused_operations.hpp` | 128 | 全文 | `:32` 魔数 seed；`:124` 公式 `ceil(work/min_chunk)+1` **未声明 `min_chunk>0` 前置**（`min_chunk==0` 即除零）；`:31` `bins{256}` 写死 | **建议** |
| 17 | `qualification/benchmarks/benchmark_common.hpp` | 332 | 全文 | `:199-202`+`:227` 空样本/均值≈0 → `median=p95=mad=cv=0` 且**无 valid 位**，「没测」与「完美一致」不可区分；`:233-236`/`:239-243` `ns<=0` 返回 0（最差吞吐）且 `mops` 有**不可达的"修正"注释自称已返回的（正确）公式为错**；`:233` NaN 不被 `ns<=0` 拦截 → 返回 NaN；`:128` `elems_per_byte` **声明后从未使用**而注释在描述它；`:135`/`:161` `s<<=1` 溢出致死循环；`:313-321` cache 阈值写死而 `:312` 注释自认「实际 cache 大小由 hwloc 提供」；`:171` 兜底 4 线程；`:304-308` `ThreadHint.threads` 被静默丢弃；`:273-281` `next_fp64` 实为 `[-1,1)` 使 `fill_dynamic_range` 文档的 1e-3 下限差 6 个数量级 | **须修** |
| 18 | `qualification/benchmarks/transfer_benchmark.cpp` | 190 | 全文 | `:136` `classify_memory_level` 用写死阈值 → E02 全部 L1/L2/L3 标签可能错标；`:132` `sizeof(T)==4?"fp32":"fp64"` 把任何其他宽度误标；`:177` `Range(4KB,256MB)` 峰值 512MB 未捕获 `bad_alloc`；`:179` `Repetitions(3)` 无统计理由；`:179` 单位 `kMillisecond` 使 4KB 档显示 0.000 不可分辨；`:94` `0xDEADBEEF`/`:95` `3.14` 语义未记录 | **须修** |
| 19 | `schemas/evidence_manifest.schema.json` | 104 | 全文 | `:25` `"branch": {"const":"feature/astrocompute-runtime"}` — **仓库当前分支为 `main`，全仓无该分支**，任何非该分支产出的 manifest 一律校验失败；`:41-43` `working_tree_clean: {const:true}` → **脏运行的诚实答案无法表达**；`:48`/`:73` 无 `minItems` → `"artifacts":[],"tests":[]` **零测试零产物通过校验**；`:87-92` enum 缺 `TIMEOUT`/`FAILED_TO_START`（ps1 实际产出这两种）；**全仓零消费者零生产者**（已实测 `grep` 无命中） | **须修** |
| 20 | `examples/cuda_axpy.cu` | 93 | 全文 | `:20-24` 打印「Fallback to CPU」但**文件内无任何 CPU 回退路径**，只 `return 0`（成功退出）；`:51`/`:52`/`:60`/`:69` 四个 `StatusCode` 全丢；`:55`/`:63`/`:72` 三行耗时经 `elapsed_since`，失败时为 `0.000 ms`；`:78` 用 `std::fabs` 但 `:7-9` 未 include `<cmath>`；**`:75`/`:78` 是本片唯一真正独立的门**（期望值为常量 5.0，非由被测代码导出） | **须修** |
| 21 | `tests/integration/test_weighted_integration.cpp` | 427 | 全文 | `:173` CpuOnly 用 `serial_ref`（由同一 `integrate_one_pixel` 生成）作期望 → **对 CPU 路径是自洽式断言**（能验分派/分块，不能验积分数学）；`:175`/`:257`/`:294` `EXPECT_EQ(coverage.done, coverage.total)` **重述 `all_done`**，非独立检查，且被 B3 的兜底清尾进一步架空；`:178-181` `GTEST_SKIP` 在 CpuOnly 块**之后**——`CorrectnessQuickAllPaths` 名为 AllPaths 实则只跑通一条；`:406-411` 若注册表无 cuda executor 则循环体一次不执行、**无任何断言发现**，`StreamConsistency` 可在两轮都只测 1 stream 时报绿；`:121`/`:130` `isfinite` + `max(ref2,1e-300)` 守卫**写法正确** | **须修** |
| 22 | `tests/unit/test_route_calibration.cpp` | 155 | 全文 | `:121-126`/`:147` 六条「Final 绝不改模型」断言**结构上不可能失败**——被检函数 `evaluate_fixed_model_on_final` 收 `const routing::RoutePath&`（`.hpp:82-84` 已核实），类型系统保证；生产侧 `route_profile_calibration.cpp:317-330` 的 `FATAL`/`abort` 守卫同样**不可达**；`:86` `EXPECT_GT(ev.count,0u)`（4 点只要求 ≥1 点产出误差）配 `.cpp:347` 的 `if (pred>0&&actual>0)` 过滤 → **筛掉真信号**；`:151-154` `ErrorGateConstants` 经核为**真门**（`.cpp:1040-1042` `median<=0.10 && max<=0.15`，四断言自洽） | **须修** |
| 23 | `tests/unit/test_route_estimator.cpp` | 368 | 全文 | `:99` `route_replay_max_slowdown_ratio = 1.0` 作为 fixture 使 `routing_trusted` 无条件成立，若流入生产将令 BDR 永久退回 OpenMP；`:134`/`:145`/`:202` 精确命中点用 0.05 容差而 `:206`/`:287`/`:292` 精确点用 `1e-9`，**容差比它要守的生产门还宽**；`:338-339` 注释自认生产「先查 op.qualified 后查场景」未被覆盖；`:253-260` `set_profile(nullptr)` → 回落 OpenMP 是**正确的 fail-closed**（正面例证） | **须修** |
| 24 | `tests/unit/test_api_traits.cpp` | 314 | 全文 | `:140`/`:147-148` `overflow`（普通 bool）与 `covered[i] += 1`（非原子读改写）被多 worker 并发写 → **检测机制自身有数据竞争**（对照 `:163` 正确用了 `std::atomic`）；`:277` `find("\"total_submitted\":0")` 加一个空格即被击穿，且只证明**全进程累计**非 0，无法区分本次走的是 `_with_desc` 还是 legacy 路径；`:238-249` 测试名 `NoProfileStillCorrect` 无任何手段保证「无画像」（默认路径 `./hardware-profile.json`） | **须修** |
| 25 | `tests/unit/test_kernel_registry.cpp` | 274 | 全文 | `:22-34` `cpu_axpy_launcher` 内用 `ASSERT_NE`/`ASSERT_TRUE`——`:133` 仅在测试线程调用故当前成立，**一旦被 runtime 派到 worker 即为 gtest 未定义行为**；`:169-196` 测试内裸 `std::thread`（局部 readers，`:196` join，非工作池，可辩护）；`:186` 注释称「保证并发读与注册真正重叠」但 `ready` 在 `:175` 递增、`find` 循环在 `:176` 之后，**重叠未被证明**；`:190-194` `register_kernel` 返回值丢弃；`:205-213` 向进程级全局注册表注册 `"kernel.global_probe"` **从不注销**；`:247-249` 只测空域 `{5,5}`，**缺反向域 `{10,5}`**——而那正是 `dispatcher.cpp:2252` 等处 `end-begin` 的风险点 | **须修** |
| 26 | `tests/unit/test_cuda_bridge_chunk.cpp` | 176 | 全文 | 两个 TEST 均 `GTEST_SKIP` 兜底 → **无 GPU 时本文件零执行且全绿**；`:61` `chunk_size==0` → `begin += 0` **死循环**；`:32-33`/`:39-41` 边界在 `int` 空间校验、索引在 `size_t` 空间 → `h>INT_MAX` 时**大片输出静默为 0**；`:154-155` `partials(2*kSpan)` 多分配一倍 span，**把潜在越界变成静默自毁**；`:74` `EXPECT_NEAR(...,1e-4)` 在 `ref≈0` 处形同虚设；`:111-114` LCG 只用第 8-17 位（周期 2^10），对空间周期性 bug 无检出能力；`:89`/`:133`/`:121`/`:175` `executor_create(0,65536,256)` 与 `dispatcher.cpp:167` 等构成 **4 处独立副本** | **须修** |
| 27 | `tests/unit/CMakeLists.txt` | 299 | 全文 | 15 个库目标**全部实测有 `add_library` 定义**（无悬空目标）；`:289-290` 退役注释与实际一致（`acr_test_resource_sustained.cpp` 确已移除）；`:10`/`:70` 在 `:74 include(GoogleTest)` **之前**调用 `gtest_discover_tests`（靠 googletest 传递引入侥幸成立）；`:266-287` `acr_test_cuda_bridge*` **不受 `ACR_BUILD_CUDA` 保护**；`:200` 生产标定实现 `route_profile_calibration.cpp` 直接编进测试 target **且同时链接 `acr_weighted_integration_example`**（双重定义风险）；全文 **0 处 `TIMEOUT`** | **建议** |
| 28 | `CMakeLists.txt` | 253 | 全文 | `:231-232` foreach 列 `eng/tools/acr_{benchmark,status,report,classic_runner}` —— **四者实测全部 MISSING**（真实路径 `tools/*`），`:235 if(EXISTS)` 使其**静默跳过**，而 `:246-250` 照样打印 `Benchmark/ Tools/ Classic runner: ON` → **4 个工具永不构建，配置输出却说它们开着**；`:18`/`:20` 称机器验收脚本 `ci/check_acr_dormant.py` —— **不存在**；`:24-25` `eng/cmake/toolchain/verify_toolchain.py` —— **不存在**；`:4` `ACSD_ENGINEERING_CONSTRAINTS.md` —— **不存在**；`:52` `ACR_FETCHCONTENT_FULL` **全树零消费**（哑开关）；`:104` `if(ACR_ENABLE_SANITIZER AND NOT MSVC)` 且启用分支外**无 else 提示**；`:106` 无 `-fno-omit-frame-pointer`、无 `float-cast-overflow`；`:108` 注释承诺的 TSan 开关**不存在**；`:57` `VERSION 0.1.0` 与仓根 `VERSION=0.1.0-alpha.1` 不一致（违反 AGENTS.md §11）；`:111-113`/`:253` 陈旧注释（「只声明不 MakeAvailable」vs `:178`/`:187`/`:199` 实际 MakeAvailable） | **阻断** |
| 29 | `scheduler/CMakeLists.txt` | 40 | 全文 | `:40` STATUS 列出 `utilization/guided_tail/memory_budget`，但这三个源文件**不在** `:4-14` 的 target 源列表 → 构建输出文件清单错误；`:24-27` 完整构建靠 `ACR_WITH_BRIDGE_LOADER` 跳过 no-op；11 个源全部存在 | 建议 |
| 30 | `backends/cpu/CMakeLists.txt` | 18 | 全文 | 6 个源全部存在；`:12` `acr_topology` 已链接——即 **hwloc/caps 信息在构建层可用**，而 `dispatcher.cpp:1140` 仍绕道 `hardware_concurrency()` | 通过（作为对照证据） |
| 31 | `core/CMakeLists.txt` | 13 | 全文 | `:5` 证实 `task_descriptor.cpp` 存在（据此**排除**了「:131 声明悬空」的假阳性）；`:3` 自认 `acr_api ↔ acr_core` **循环依赖**，靠链接行重复库消解 | 建议 |
| 32 | `examples/CMakeLists.txt` | 16 | 全文 | `:2-16` 无条件构建全部示例，**`ACR_BUILD_EXAMPLES`（父文件 `:47` 声明，默认 ON）在本文件从未被检查** → 设 `OFF` 无任何效果 | 建议 |
| 33 | `tests/sanitizer/run_acr_sanitizers.ps1` | 197 | 全文 | 见 **B2**；另 `:17-20` 四条默认路径写死 `F:\Astro dev\**Astro CS Normalization Database**\run\...\lib\acr\...` —— 仓库已改名、树已迁 `lib/infrastructure/acr`、`run/logs` 不存在 → **四条默认全失效**；`:18` 日志目录写死 `20260805`（头 `:11` 承诺 `<YYYYMMDD>`）→ 每次运行覆盖上次证据；`:96`+`:139-140` 未传 `--error-exitcode`，**compute-sanitizer 的 `.cslog` 被采集却从不参与判定**；`:90-91` `catch{}` 后调无超时 `WaitForExit()` → 可永久挂起（违反本文件 `:10` 「任何外部进程都带明确 timeout」）；`:98-103` `Kill` 抛异常会把已设的 `TIMEOUT` 覆写成 `FAILED_TO_START`；`:32` 检查 `.bat` 却执行 `.exe`；`:61-66` `EnvPair` 格式不符时静默跳过 | **阻断** |
| 34 | `tests/classic/e17_model_fit.cpp` | 337 | 全文 | **自洽式断言（本片最重之一）**：`:71` 训练点值 = `model(s)`，`:84` 验证点「期望值」= **同一个** `model(mid)`，`:116-120` 再拿 `curve.predict()` 去比 —— **期望量与被检量出自同一套定义式，恒绿**。文件名为「Hardware Profile 拟合验证」但**全程无任何真实硬件测量**，`:207` 起却把 PASS 写进 `ResultSink` 作为资格证据；`:191` `pass` **只看 `median`**，`max_rel_err` 算了、报了、从不参与判定；`:113` 空 holdout → 全 0 → `0<=0.15` → **PASS**；`:119-123` actual 与 pred 同时近 0 时 `rel_err` 留 0.0；`:209`/`:225`/`:240`/`:303`/`:321` `TimingStats{median_rel_err * 1000.0, 0.0}` 把**无量纲相对误差写进时间字段**；`:78-90` `s<<=1` 溢出致死循环 | **阻断** |
| 35 | `tests/classic/e08_scan.cpp` | 284 | 全文 | `:211-233` `run_max_scan` **完全不调用任何 ACR API** —— ref 与 out 都是测试内手写串行循环，`E08Scan.Max1K` 实测的是「串行 vs 串行」，却是被上报的 benchmark；`:182-187` **符号反转门** `demonstrates_dependency = (!exact) && (n>block_size)`——ACR 真缺陷（`parallel_chunks` 只跑 block 0）会让本测试**变红**，与测试自身预期红不可区分；`:92`/`:102` `block_size==0` → 整数除零；`:206` 容差锚在答案幅值，前缀和落在近 0 时容差塌到与参考自身 fp32 噪声同量级；`:215` `in[0]` 与 `:206` `ref.back()` 均无 `n==0` 守卫（u64 版 `:82`/`:126` 有） | **须修** |
| 36 | `tests/classic/e04_transpose.cpp` | 220 | 全文 | `:82`/`:145` `max_abs <= 1e-6`，而行内注释自写 `// 转置应 bit-exact`、文件头 `:3` 写「逐元素 exact」——**判据比自述松 6 个量级**；同文件 uint32 孪生路径 `:117-118` 用的是严格相等，**同文件两套标准**；`:68-75` 循环上界用运行时 `tw/th`、基址却用捕获的 `tile_w/tile_h` 自行重算，一致性只靠未文档化的约定；`:73` 界检查只防越界、**不防索引错** | **须修** |
| 37 | `tests/classic/e10_mandelbrot.cpp` | 184 | 全文 | `:68` `max_abs == 0.0`（整数 exact）是**真门**且 SUT 正确（`parallel_for_2d` 的调度与坐标映射），但参考循环 `:32-36` 与 lambda 体 `:57-61` **文本相同却可能被不同优化（向量化/FMA 收缩）**，Mandelbrot 对 1 ulp 敏感 → 迭代计数翻转可致误红，且文件内**无任何 FP 契约**；`:46` `region_label` 形参**从未被使用**（头 `:8` 承诺「画像记录 uniformity」实际未进 `CaseResult`）；`:47`/`:38` `w*h` 无检查，32 位下回绕；`:79` 注释称 `kFastEscape`「远离集合，大部分点 1-5 次逃逸」但该框 `-2.5..1.0` **包含整个 Mandelbrot 集**；`:85` `kCardioid` 的 x 区间 `-0.5..0.3` 一侧是逃逸区 | 建议 |
| 38 | `tests/classic/classic_main.cpp` | 61 | 全文 | `:2` 头注释称「显式 runtime_init/**shutdown**」，但 `:59-60` 只有 `RUN_ALL_TESTS()` + `exit_after_tests(result)`，**无 `runtime_shutdown()`**；`:26`→`:29` 由 `run_e13` 直跳 `run_e18`，**e14/e19 两槽既不声明也不引用**（与片外 `tests/classic/CMakeLists.txt:3,18`「run_e14 接口保留为空占位」直接矛盾——该占位**从未存在**） | 建议 |
| 39 | `profile/profile_reader.hpp` | 76 | 全文 | `:7` 把 Stale 定义为「警告 + 继续运行（不强制重新 benchmark）」——**这是被写成契约的 fail-open 设计**，`.cpp:577-581` 照此实现并返回指纹不匹配的画像；`:31` 默认路径注释与 `.cpp:419` 一致；`:44` 声明「不触发加载」但 `.cpp:553` 无锁读 | 须修（设计口径） |
| 40 | `examples/weighted_integration/weighted_integration_kernels.hpp` | 20 | 全文 | `:13-15` `weighted_integration_openmp(v, output, int threads)` 带显式 `int threads` 形参（OpenMP `num_threads` 惯用法）；所含 `weighted_integration_common.hpp` **不在本片**，故 `integrate_one_pixel` 求和顺序与 `generate_synthetic` 数据分布**无法在本片核验**——而 `test_weighted_integration.cpp` 的全部容差建立在它们之上 | UNRESOLVED（需 INF-acr-002 的 `.cpp`） |
| 41 | `docs/ADR-003-hwloc.md` / `ADR-004-cpu_features.md` | 70 / 76 | 全文 | `:7` 指向 `docs/dependency-lock.json` — **已核实存在，引用有效**（我一度按错误路径查证，纠正后确认非悬空）；`:15`/`:48-49` hwloc 为**唯一**拓扑来源、`hardware_concurrency` **未采用**——`dispatcher.cpp:1140`/`:1121`/`:476`、`profile_reader.cpp:382`、`benchmark_common.hpp:170` 逐条违反；`:29`/`:53-54` cpu_features 为**唯一** ISA 来源、`__builtin_cpu_supports` **未采用**——`CMakeLists.txt:214-222` 恰恰把它设为 fallback，且 ADR 自述「MSVC 不支持」；`:73` 验收「MSVC + GCC/Clang 均无错误」在 MSVC 上无任何可用 ISA 检测源 | **须修**（ADR 与实现相悖） |
| 42 | `docs/ADR-008-cmake-fetchcontent.md` | 82 | 全文 | `:29` 决策「**所有**第三方库通过 FetchContent 拉取」、`:60-61` 把 **MSYS2 pacman 列为「未采用」**——而 `CMakeLists.txt:170-200` 实现的是「find_package(MSYS2) **优先**、FetchContent 回退」，**与 ADR 直接相反**；`:34`/`:51` 称版本锁定「CMake 脚本从中读取」且「CI 校验」——`CMakeLists.txt:119-128` 的 10 个 GIT_TAG **全为内联字面量**，全文无任何读取锁文件的指令；`:169` 注释称实际用 `tbb 2023.0.0 / gtest 1.17.0`，字面量却钉 `v2022.0.0 / v1.15.2` | **须修** |

---

## 4. 发现清单

### 阻断（7）

| 编号 | 位置 | 缺陷 | 归类 |
|---|---|---|---|
| **B1** | `cuda_backend.cu:102-103,172`（+`:104`） | `call_once` 使 `result` 保持 `Ok`；首次失败后**每次调用报成功**。头 `cuda_backend.hpp:47-49` 明确承诺返回真实状态 | **自愈判据**（第一跑红、第二跑绿、缺陷仍在）+ 静默降级 |
| **B2** | `run_acr_sanitizers.ps1:192`（+`:101,:128,:157`） | 失败集漏 `SKIPPED`/`FAILED_TO_START`；exe 缺失时连记录都不产生 → **什么都没跑也 exit 0 并打印 "all recorded"**；直接违反本文件 `:10-11` | 恒真门 + 悬空引用 |
| **B3** | `dispatcher.cpp:1671-1713`（对 `:1259-1264`/`:1282`/`:1394`/`:1536`/`:1720`） | 兜底清尾把永久 `Fail` 与 `gate_aborted` 的语义**整个推翻**：绕过内存检查与 staging 记账，一次领走全部剩余 | 静默降级（fail-open）+ 记账与行为相悖 |
| **B4** | `dispatcher.cpp:1181-1186,1158,1665`（规模 `:1140`） | 裸 `std::thread` 私建池，不归任何符号所有；同文件另两条路径走 `parallel_batch`；规模取自 ADR 明文否决的 `hardware_concurrency()` | **私建线程池** |
| **B5** | `CMakeLists.txt:231-232,235`（对 `:246-250`） | 4 个工具目录路径写错（`eng/tools/*` vs 真实 `tools/*`）→ 静默跳过、永不构建；而配置摘要照印 `ON` | 悬空引用 + 恒真门（配置输出撒谎） |
| **B6** | `dispatcher.cpp:2390-2397,2435-2442`（对 `:5-7` 头注释、`:2599-2605`） | 调完 `legacy_parallel` **无条件**写 `all_done=true / done_blocks=1 / items_done=n`，再由此反推 coverage 并强制 `failed=0`。子代理 C 实测该路径**有活调用者**（`weighted_integration_kernels.cpp:157` 注册、`dispatcher.cpp:2393/2438` 调用、`test_dispatcher_bdr.cpp` 6 处断言）→ **是主执行路径，不是退役代码** | 自洽式断言 + 退役声明被证伪 |
| **B7** | `memory_budget.cpp:107`（对 `:328` `dispatcher.cpp` 的 `StagingLedger`） | `pinned_used = min(used_ram, pinned_limit)` **构造性地恒不超过限值** → 任何 `pinned_used>limit` 门永假；`sample()` 从不设 `pinned_exceeded`。同一物理量存在**伪造源**与**真实 ledger** 两个真相源 | **恒红门（反向）**+ 自洽式断言 |

### 须修（24，择要列全）

1. `avx512.cpp:44-46` / `sse.cpp:39-41` — 门禁只验硬件能力、不验是否编进了 SIMD；MSVC 下走标量仍返回 `true`。
2. `profile_reader.cpp:95-112` — double→整型无范围检查，`-1`/`1e999` 为 UB；全树无 `float-cast-overflow`。
3. `profile_reader.cpp:552-562` / `system_metrics.cpp:445-451,480-482` / `memory_budget.cpp:38-40,102-105,158-159` — 无锁读被并发写的状态（数据竞争）；三处头文件均自称线程安全。
4. `system_metrics.hpp:3` vs `system_metrics.cpp:351-356` — NVML 读失败标 `valid=true`；同文件 `:420-422` 相反做法。
5. `system_metrics.cpp:179-186,200-204` — 枚举失败/零句柄仍对外报 `nvml_available=true`。
6. `system_metrics.cpp:183-186` — 注释称「保留 dll 以便后续重试」，但 `:130` 后标志永不复位，**重试不可能发生**。
7. `system_metrics.cpp:473-478` — `noexcept` 函数内 `lock_guard`，锁失败即 `std::terminate`（项目禁模块直接终止进程）。
8. `cuda_buffer.hpp:29-37` — 分配失败把 `count_` 归零，容量信息被销毁且错误码被改写成 `OutOfBounds`。
9. `cuda_buffer.hpp:147-151,117,141,144` — CUDA 计时/事件返回值全丢；失败读作 `0.0 ms`（性能最好值）。
10. `dispatcher.cpp:2319-2348` — `BdrCacheKey` 不含 RAM/VRAM 余量；`:2317` 把「取不到数据」解释为「无限制」并在无采样器时仍缓存决策。
11. `dispatcher.cpp:258,269,1857,1880,2165,2194,2227,2285,2378` + `residency_manager.hpp:43` — 设备身份与驻留跟踪写死 `"cuda:0"`；多 GPU 下报告与驻留判定均错。
12. `e17_model_fit.cpp:71,84,116-120` — 自洽式断言：期望值由被测模型自身生成，且全程无真实硬件测量却写入资格证据。
13. `e17_model_fit.cpp:191,113,209,321` — 只判 median；空 holdout 判 PASS；相对误差写进时间字段。
14. `test_route_calibration.cpp:121-126,147` — 六条断言结构上不可能失败（被检函数收 `const&`）；生产侧守卫同为不可达死码。
15. `test_route_calibration.cpp:86` + `.cpp:347` — 只要求 4 点中 ≥1 点产出误差，模型在自己最差点被筛掉时仍胜出。
16. `benchmark_common.hpp:199-202,227` — 空样本/均值≈0 → 全 0 统计且无 valid 位。
17. `benchmark_common.hpp:239-243` — 不可达的「修正」注释自称正确的返回式为错，会诱导后人改坏正确代码。
18. `benchmark_common.hpp:313-321` — cache 阈值写死而注释自认应由 hwloc 提供（连带 E02 层级标签）。
19. `benchmark_common.hpp:128` / `:135,161` / `:304-308` — 死参数、溢出死循环、`ThreadHint.threads` 静默丢弃。
20. `evidence_manifest.schema.json:25,41-43,48,73,87-92` — 分支名钉死为 `const`（仓库在 `main`）；`working_tree_clean: const true` 使脏运行无法表达；无 `minItems`；enum 缺两种实际状态；**零消费者**。
21. `CMakeLists.txt:18,20,24-25,4` + `:52,104,106,108,57` — 三处「机器验收」脚本/规范全部不存在；哑开关；MSVC 下 sanitizer 静默失效；无 `-fno-omit-frame-pointer`/`float-cast-overflow`；承诺的 TSan 开关不存在；版本号与仓根不一致。
22. `docs/ADR-008:29,60-61` vs `CMakeLists.txt:170-200`；`ADR-003:48-49`、`ADR-004:53-54` vs `CMakeLists.txt:214-222` — **三处 ADR 决策被实现逐条反做**。
23. `e08_scan.cpp:211-233,182-187` — `run_max_scan` 不调用任何 ACR API 却作为 benchmark 上报；符号反转门把「算错了」记成 PASS 写进证据流。
24. `tests/sanitizer/CMakeLists.txt:21-25`（片外，只读核对）— ASan target 只编 4 个源，**`dispatcher.cpp` 不在其中**；`run_acr_sanitizers.ps1:4-6` 自述亦承认排除 Dispatcher。即本片缺陷最密集的文件**零内存消毒覆盖**，而唯一存在的消毒证据链按 B2 恒绿。

### 建议（择要）

`operation_profile.hpp:11` 悬空 schema · `focused_operations.hpp:124` 缺 `min_chunk>0` 前置 · `task_descriptor.hpp:104-112` reduce 构造器是 range 的逐字节别名 · `runtime_internal.h:36` 取消仍报 `Ok`、`:44-50` `wait()` 无超时 · `e04_transpose.cpp:82,145` 注释说 bit-exact 而门判 1e-6 · `test_api_traits.cpp:140,147-148` 检测机制自身 racy · `test_kernel_registry.cpp:22-34` launcher 内用 ASSERT、`:186` 重叠未证明、`:205-213` 全局注册表污染 · `test_cuda_bridge_chunk.cpp:61` `chunk_size==0` 死循环、`:154-155` 多分配一倍 span · `cuda_axpy.cu:20-24` 谎报已回退 CPU · `examples/CMakeLists.txt` 开关失效 · `scheduler/CMakeLists.txt:40` STATUS 文件清单错误 · `core/CMakeLists.txt:3` 自认循环依赖 · `classic_main.cpp:2` 缺 `runtime_shutdown` · `transfer_benchmark.cpp:136,177,179` · `test_route_estimator.cpp:99,134` · `e10_mandelbrot.cpp:46,79,85` · 11 个 `NN_*.md` 规范文档在仓内零命中。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **CE-1** | 在无 CUDA 设备的机器上调用 `CudaBackend::instance().initialize()` **两次**，检查返回值 | 「幂等调用不会丢失失败状态」 | **推翻成功**。第一次返回 `DeviceLost`；第二次 lambda 不执行，`result` 停在 `:102` 的 `Ok` → **返回 `Ok`，而 `available()` 为 false**。头 `cuda_backend.hpp:48` 承诺的四态返回被违反。 |
| **CE-2** | 在**没有任何 ACR 构建产物**的机器上执行 `run_acr_sanitizers.ps1` | 「一个 sanitizer 都没跑会报红」 | **推翻成功**。`$MsvcAsanExe` 不存在 → 1 条 `SKIPPED`；`acr_test_cuda_bridge_chunk.exe` 与 `acr_test_cuda_bridge.exe` 不存在 → `:128`/`:157` 的 `if` 为假，**零条记录**。`$failed` 为空 → **exit 0** 并打印 `"all recorded (1 items)"`。与本文件 `:10-11` 的「绝不冒充通过」正面冲突。 |
| **CE-3** | 让 `MemoryBudgetController::suggest_action` 连续返回 `Fail`，观察 `dispatch_invocation` | 「永久 Fail 会真正停止剩余工作」 | **推翻成功**。所有 worker 在 `:1282` break；join 后 `:1671` 的兜底循环用 `claim_next_dynamic(exec->id(), rem)` 把**剩余全部**领走并 `exec->submit` 执行完毕。即 `Fail` 语义被完全推翻，且 `:1394` 的内存检查、`:1536` 的 staging 记账**全程未执行**。 |
| **CE-4** | 构造 `hardware-profile.json` 使 `"available_memory_bytes": -1` | 「解析器只吃合法 JSON」 | **推翻成功**。`:74` 接受 `-`、`:76` 见数字 → `seen_digit=true`；`:91` `stod("-1")` 成功；`:112` `static_cast<std::size_t>(-1.0)` 值不可表示 → **UB**（实测常得 `0x8000…0`）。同形：`1e999` 经 `stod` 得 `+HUGE_VAL` **不抛**，直接进 `peak_bandwidth_gbps`。 |
| **CE-5** | 把 `kernel_avx512_axpy` 的尾部循环写成漏更新 `y[i]`（在支持 AVX-512 的机器上） | 「AVX-512 kernel 的结果正确性有常驻测试」 | **推翻成功**。片内 `avx512.cpp` **零结果正确性门**；唯一的 `*_safe` 测试包在 `if (!caps.has(AVX512F))` 内，在支持 AVX-512 的机器上**测试体一次不执行却报 PASS**。`kernel_sse_axpy_safe` 全仓**零测试引用**。 |
| **CE-6** | 令 `evaluate_fixed_model_on_final` 试图改写 `path`（例如把模型换掉） | 「`FinalHoldoutNeverMutatesModel` 会在模型被改时变红」 | **构造失败——这正是恒真门的定义**。被检函数收 `const RoutePath&`，任何改写都是**编译错误**，红不可能出现。生产侧 `:317-330` 的 `FATAL`+`abort` 同样不可达。 |
| **CE-7** | 令 `predict_path` 对 loglog 模型只在它最差的 2 个 probe 点返回 `-1` | 「模型选择门会因覆盖丢失而变红」 | **推翻成功**。`.cpp:347` 的 `if (pred>0 && actual>0)` 把这 2 点整点丢弃 → loglog 的 `errs` 不含自己的败笔 → 仍胜出 → `test_route_calibration.cpp:88` 照样绿。**该测试无法区分「loglog 真更优」与「loglog 在自己最差点被筛掉才显得更优」**。 |
| **CE-8** | 令 E17.4 在 1MB↔2MB 中点产生 ~29% 误差（实测：actual≈2224370、pred≈1573489） | 「median ≤ 15% 的门会守住 crossover」 | **推翻成功**。14 个 holdout 点取中位数，**单个 29% 离群点推不动 median** → PASS。`max_rel_err` 被算出、被上报，**全仓无一处与阈值比较**。失败信息却写着「> 15% threshold **at crossover**」——**声称守的正是它没守的**。 |
| **CE-9** | 令 `make_train_validate` 的 `min_bytes == max_bytes` | 「没有留出点时不会误判」 | **推翻成功**。循环跑一次、不产生 validate 点 → `:113` 早返回全 0 → `0.0 <= 0.15` → **PASS**，并写入 `ErrorStats{0,0,0}` + `"PASS"`，`n_holdout=0`。**「没有数据」被记成「0% 误差」**。 |
| **CE-10** | 把 `cpu_axpy_launcher` 的 `y[i] = *a*x[i] + y[i]` 改成 `y[i] = *a*x[i]`（丢累加） | 「CPU AXPY 的正确性有独立证据」 | **部分推翻**。`test_kernel_registry.cpp:136` 的期望是**常量 5.0**（`3*1+2`），是**独立门**，会红——这一条我判为该文件的正面项。但片外 `test_topology.cpp:165-168` 的 `kernel_axpy_scalar` vs **同函数再调一次**（子代理 B 独立确认）是纯自洽门，会绿。即：**片内独立、片外自洽，整链仍有缺口**。 |

**红队自证（我尝试后判定「无缺陷」，不虚报）**：

- `task_descriptor.hpp:131/134` 声明「实现在 task_descriptor.cpp」——已 `git ls-files` 核实**文件存在**，**排除**悬空引用。
- `core/CMakeLists.txt:5` 同样证实。
- `docs/ADR-003:7`/`ADR-004:7`/`ADR-008:7` 指向 `docs/dependency-lock.json`——我最初按 `acr/dependency-lock.json` 查证得出「不存在」，**自查纠正后确认该文件存在**，引用有效，**排除**。
- `CMakePresets.json` 的 `win-msvc-17.14.39-x64` / `linux-control`——**已核实存在**，`CMakeLists.txt:23` 引用有效。
- `tests/unit/CMakeLists.txt` 的 15 个库目标——逐个 `add_library` 实测，**全部有定义**，**排除**「目标悬空」。
- `partitioner.cpp`、`residency_manager.cpp`、`cuda_buffer.cpp`、`route_profile_v2.cpp` —— 全部存在，相关声明非悬空。
- `avx512.cpp:41-43` 的 AVX-512 子集独立校验 —— **符合 ADR-004:21/32**，判**通过**。
- `e08_scan.cpp:222` 用 `numeric_limits<float>::lowest()`（而非 `-inf`）作 max 单位元 —— **正确**，阻断 NaN 传染，判**通过**。
- `system_metrics.cpp:285-293` 的 `delta_total==0` 早返回并标 `valid=false` —— 比 `memory_budget.cpp` 写得好，判**通过**。
- `test_route_calibration.cpp:151-154` 的 `ErrorGateConstants` —— 初看像恒红门，经与 `.cpp:1040-1042` 比对后确认**是真门**，**排除**（采纳子代理 B 的排除结论）。
- `classic_main.cpp:59-60` 调 `exit_after_tests(result)` —— 初看像「退出码恒 0」，经子代理 B 核实 `tests/fault/exit_safe.hpp:15-17` 为 `std::_Exit(result)`，**排除**。
- `test_weighted_integration.cpp:226-227` 的 `fdata`/`w` —— 我一度判为张冠李戴，细读后确认是**块内有意遮蔽外层同名变量**，内外一致，**排除**。
- `dispatcher.cpp:2097-2098` 的 coverage 同三元式合成 —— 初看是自洽门，但经核对**现役测试均走 `coverage_from_pool`**（`:175` CpuOnly、`:257`/`:294` mixed_pool），该组合未被引爆 → 降为**建议**而非阻断。

---

## 6. 盲复算

**方法**：我在**未查看任何既有审稿结论**（`审稿-RR*.md` / `审稿-R2-*.md` / `审稿-R3-*.md` / `审稿-P1-*.md`）的前提下完成全部 42 份阅读与结论推导；子代理亦在同一前提下独立工作。结论产出后才回头核对。

**回查结果**：`分片清单/逐份判定-权威版.csv` 对本片 42 份的 `reason` 列**全部为空**，且该表根本没有「判定/等级」字段（只有 `path,lines,ext,layer,tier,slice,reason`）。因此：

- **无既有机器判定可对齐** —— 不存在被既有结论带偏的可能，也不存在「与既有结论一致」的凭据。
- **我改用更有意义的盲复算基线：源码自身写下的断言**（头文件契约、ADR 决策、schema 声明、脚本头注释、配置摘要输出）。这些是「现行结论」的具体载体，红队姿态即是默认它们为错。

**据此独立取证，判与「源码自述」不一致的项**：

| 源码自述 | 位置 | 我的独立重算 | 判定 |
|---|---|---|---|
| 「返回 Ok / DeviceLost / OutOfMemory / KernelFailed；幂等」 | `cuda_backend.hpp:47-49` | 第二次起恒返 `Ok` | **不一致（偏严于自述）** |
| 「工具缺失…记录为 SKIPPED，**绝不冒充通过**」 | `ps1:10-11` | 全 SKIPPED → exit 0 | **不一致（偏严）** |
| 「coverage…不无条件 mark_done」 | `dispatcher.cpp:7` | OpenMP 路径无条件写 `all_done=true` | **不一致（偏严）** |
| 「hwloc 为唯一拓扑来源；`hardware_concurrency` 未采用」 | `ADR-003:15,48-49` | 5 处在用 | **不一致（偏严）** |
| 「cpu_features 为唯一 ISA 源；`__builtin_cpu_supports` 未采用」 | `ADR-004:15,29,53-54` | CMake 把它设为 fallback | **不一致（偏严）** |
| 「所有第三方库通过 FetchContent；MSYS2 pacman 未采用」 | `ADR-008:29,60-61` | 实现为 MSYS2 优先 | **不一致（偏严）** |
| 「阈值是经验值，实际 cache 大小由 hwloc 提供」 | `benchmark_common.hpp:312` | 用通用阈值 | **不一致（偏严）** |
| 「无 GPU 时明确标记 estimated=true，**不伪报**」 | `system_metrics.hpp:12` | 失败标 `valid=true` | **不一致（偏严）** |
| 「线程安全（内部用 mutex 保护）」 | `system_metrics.hpp:13,59` 等三处 | 多处无锁读 | **不一致（偏严）** |
| 「转置应 bit-exact」 | `e04_transpose.cpp:82` | 门判 `≤1e-6` | **不一致（偏严）** |
| 配置摘要 `Benchmark/ Tools/ Classic runner: ON` | `CMakeLists.txt:246-250` | 4 个 target 永不构建 | **不一致（偏严）** |
| 「run_e14 接口保留为空占位」 | 片外 `tests/classic/CMakeLists.txt:3,18` | 全仓零定义 | **不一致（偏严）** |
| 「CPU/GPU 利用率控制已移除：不再采样」 | 片外 `dispatcher.hpp:110` | `memory_budget.cpp:118` 仍在生产路径采 VRAM | **不一致（偏严）** |

**方向判定：整体偏严。** 12 项不一致中 12 项都是「源码自述比实现更好」，无一项是「实现比自述更好」——即本片的声明质量系统性高于实现质量。这与负责人「看到任何『检查通过』的机制，不要据此认为实现正确」的裁定一致：**本片的自述层不可作为通过依据**。

**我可能过严的地方（主动标注，供交叉复核）**：

1. `dispatcher.cpp:258`/`:269` 的 `"cuda:0"` 在**当前单 GPU 部署形态下不产生可观测错误**。我按多 GPU 正确性判为须修；若项目已冻结单 GPU，应降为建议。
2. `cuda_buffer.hpp:31` 的 `count*sizeof(T)` 溢出需 `count > 2^62` 才可达，现网不可达。我判须修是就缺失守卫本身而言。
3. `system_metrics.cpp:473-478` 的 `noexcept`+`lock_guard` 触发 `terminate` 需 mutex 锁失败（近乎不可达）。我按「项目禁模块直接终止进程」的口径判须修。
4. 子代理提出的 `benchmark_common.hpp:273-281` 分布差 6 个数量级、子代理 C 的 `hardware_profile.schema.json` 全线冲突等，我**未亲自复算**，故**未纳入本片阻断/须修**，仅在建议中转述并标注来源。

---

## 7. 子代理派发记录

**派发 5 个**，均只读、零 git 写、不编译、不跑二进制、不得读 `/tmp/acsd_g08/`，结论须来自 `read` 原文并给 `文件:行`。

| 代理 | 任务 | 结论 |
|---|---|---|
| **A** | 私建线程池 + 静默降级全片扫查 | 42/42，9920 行。定位 `dispatcher.cpp:1181-1186` 裸 `std::thread` 池、**不归任何符号所有**；45 条静默降级；8 项 UNRESOLVED |
| **B** | 自洽式断言 / 恒真门 / 恒红门 / 筛掉真信号 | 42/42，9920 行。21 项门清单；两条头条（`test_route_calibration` const& 恒真门、`.cpp:347` 筛掉真信号）+ 8 个构造反例 |
| **C** | 悬空引用 + CMake 目标 + schema 键 + 退役声明 | 42/42，9920 行。4 个 P0 悬空（含 `eng/tools/*`）、3 个 schema 全线不匹配、**2 条退役声明被证伪** |
| **D** | 硬编码数值 + 数值稳定性 | 42/42，9920 行。硬编码表 80+ 条、数值稳定性 57 条、16 个反例（14 推翻成功） |
| **E**（误派） | 与 A 完全相同的任务（我的调用失误，重复派发） | 独立复现了 A 的线程池与静默降级结论，构成一次无偏的重复验证 |

### 逐条复核：采纳 / 修正 / 否决

**我亲自 `read` 原文复核并采纳（子代理结论 = 我的结论）**：

- A 的 `dispatcher.cpp:1181-1186` 私建池 → 我读到同一段，采纳为 **B4**。
- A 的 `run_acr_sanitizers.ps1:192` 失败集漏项 → 我读全文 197 行，采纳为 **B2**，并**补充**了我独立发现的 `:18` 日志目录写死 `20260805`、`:32` 查 `.bat` 执行 `.exe`。
- A 的 `cuda_backend.cu` `call_once` → 我读全文，**采纳并升级为 B1**，并**补充**了头文件 `:47-49` 的契约原文作为判据。
- A 的 `system_metrics.cpp:351-356` → 采纳；并**补充**了我发现的 `:179-186` 对外报 healthy、`:183-186` 注释称可重试而实际不可能。
- B 的 `test_route_calibration.cpp:121-126` 恒真门 → 我**独立**打开 `route_profile_calibration.hpp:82-84` 确认 `const&`，采纳为须修 #14，并**补充**了生产侧 `.cpp:317-330` 的 `abort` 守卫同为死码。
- B 的 `.cpp:347` 筛掉真信号 → 我独立读到该行，采纳为须修 #15。
- B 的 `avx512.cpp`/`sse.cpp` 结果正确性零覆盖 → 我读两文件全文，采纳为须修 #1 与反例 CE-5。
- C 的 `eng/tools/*` 四目录悬空 → 我**亲自跑了** `for` 循环验证 4 个 MISSING 并 `git ls-files` 确认真实路径为 `tools/*`，采纳为 **B5**。
- C 的 `run_e14` 占位被证伪 → 我读 `classic_main.cpp:26-29` 确认跳空，采纳为建议。
- C 的 `legacy_parallel` 有活调用者 → 我读 `dispatcher.cpp:2390/2393/2435/2438` 确认调用，**并据此把 A 标为「兼容路径」的说法升级为 B6 阻断**。
- C 的 `check_acr_dormant.py` / `verify_toolchain.py` / `ACSD_ENGINEERING_CONSTRAINTS.md` 三处不存在 → 我**亲自 `-e` 验证**三个路径均 MISSING，采纳为须修 #21。
- D 的 `mops()` 不可达「修正」注释 → 我读原文，采纳为须修 #17。
- D 的 `compute_stats` 空样本/均值≈0 → 我读 `:199-202`/`:227` 采纳为须修 #16。
- D 的 `log2_size_sequence_bytes` 死参数 → 我读 `:126-141` 确认 `elems_per_byte` 确未出现，采纳。
- D 的 `memory_budget.cpp` RAM 读失败 → Fail 链 → 我**独立复核** `:69` 与 `:58` 并与 `dispatcher.cpp:604-605` 的守卫路径对照，采纳并**并入 B7 的分析链**。

**我否决 / 降级 / 修正的**：

| 代理结论 | 我的处置 | 理由 |
|---|---|---|
| A：「`test_kernel_registry.cpp:169-196` 裸 `std::thread`」 | **降级为建议** | 读原文后确认是局部 `readers` 观察者、`:196` join、非工作执行池。按「池所有权」判据不构成私建池——不替它开脱也不夸大 |
| A：「`benchmark` 被 Declare 但从未 MakeAvailable，transfer_benchmark 可能无法构建」 | **否决**（不下结论） | 他自标 UNRESOLVED；`qualification/CMakeLists.txt` 不在本片且我未读该文件，无证据即不列缺陷。已在 UNRESOLVED 中转述 |
| A：「`e10` 用 1 悄悄替换除零」 | **降级为建议** | 读 `e10` 全文确认 `(w>1?w-1:1)` 分母恒 ≥1，**无除零**，非阻断 |
| B：「`classic_main.cpp:59-60` 退出码恒 0」 | **否决**（B 自己已排除，我复核确认） | `std::_Exit(result)` 退出码正确。采纳 B 的排除 |
| B：「`ErrorGateConstants` 是恒红门」 | **否决**（B 自己已排除，我复核确认） | `.cpp:1040-1042` `median<=0.10 && max<=0.15`，四条断言自洽，是真门 |
| B：「`dispatcher.cpp:2097-2098` coverage 自洽合成」 | **降级为建议** | 读后确认现役测试均走 `coverage_from_pool`，该组合未引爆 |
| B：「`test_topology.cpp:165-168` 自洽门」 | **转述为片外**，仅引用 | 该文件**不在本片**，我不据它给本片定阻断；但它是 CE-10 的背景，已如实标注 |
| C：「`hardware_profile.schema.json` 与代码全线冲突」 | **不纳入本片判定** | `profile_generator.cpp` 与该 schema 均不在本片，我只读过 reader 侧。转述为建议并标注需 INF-acr-002/003 复核 |
| C：「11 个 `NN_*.md` 全部不存在」 | **纳入建议**（未升级） | 我本人未逐个复算 11 个 grep；子代理给出命令与 `tracked_matches=0`，可信但我只承担自己复核过的部分 |
| D：`e10_mandelbrot.cpp` FP 收缩无契约 → 可能误红 | **降级为建议** | 属实但属「可能误红」而非「漏报真缺陷」，与本轮重点（漏报）方向相反 |
| D：`benchmark_common.hpp:273-281` 分布差 6 个数量级 | **不纳入**，仅转述 | 我未亲自复算 `next_fp64` 的位域，不据他人结论定级 |
| D：`test_route_calibration.cpp:114` holdout 泄漏 | **降级为建议** | 属实但影响面小 |
| A/D/E 共同：「`ACR_ENABLE_STARPU` / `ACR_FETCHCONTENT_FULL` 是哑开关」 | **只采纳 `ACR_FETCHCONTENT_FULL`** | 我亲自 grep 确认其全树零消费；`ACR_ENABLE_STARPU` 属 INF-acr-002 的 `tools/`，我只在自己片内文件里确认了 `ACR_FETCHCONTENT_FULL` |

**我自己发现、子代理未报或未升级的（本轮增量）**：

1. **B3（dispatcher 兜底清尾推翻 Fail）** —— 无人提出；我读 `dispatcher.cpp:1666-1713` 与 `:1259-1264` 对照得出，并自行构造 CE-3 验证。**这是本片最重的新发现之一。**
2. **B5 的「配置摘要仍打印 ON」** —— C 只报了目标不构建，我补了 `:246-250` 输出撒谎这一层（恒真门）。
3. **`:189`/`:1943` `65536`、`kMaxAttempts` 三处各自定义、200ms 与 100ms 两套内存采样窗并存** —— D 标为硬编码，我补了「同一常量多处副本、改一处即行为分裂」这层。
4. **`system_metrics.cpp:183-186` 注释称可重试而 `nvml_init_attempted` 永不复位** —— 无子代理提出。
5. **`cuda_buffer.hpp:77/94` `n==0` 先于空指针检查返回 `Ok`** —— 无子代理提出。
6. **`test_cuda_bridge_chunk.cpp:154-155` 多分配一倍 span 把越界变成静默自毁** —— D 提到，我只把它与「筛查子集掩盖最大项」这一判据类型对上，纳入须修。
7. **`e08_scan.cpp:211-233` `run_max_scan` 不调用任何 ACR API** —— 无子代理提出（他们聚焦自洽门，漏了「压根没测被测对象」这一更基本的形态）。
8. **`tests/sanitizer/CMakeLists.txt:21-25` 不含 `dispatcher.cpp`** —— 无子代理明确提出；我读该文件后得出，与 B2 叠加后构成本片最完整的证据链失效叙事。

---

## 8. 自证段（可复跑命令）

前置：`cd "/workspace/Astro CS Database"`；所有命令**只读**，不编译、不跑测试、不写仓内文件。中文路径一律已用 `git -c core.quotepath=false`。

```bash
# ── 覆盖口径：逐文件行数求和，应得 9920（与权威清单一致）
git -c core.quotepath=false log -1 --format='%H'          # 期望 850a9edefd47…
sed -n '2664,2706p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | sed 's/^ *- "//; s/"$//' > /tmp/inf_acr_001_files.txt
: > /tmp/inf_acr_001_files.txt
sed -n '2665,2706p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | sed 's/^ *- "//; s/"$//' > /tmp/inf_acr_001_files.txt
t=0; while IFS= read -r f; do
  n=$(git -c core.quotepath=false show HEAD:"$f" | wc -l); printf "%6d  %s\n" "$n" "$f"; t=$((t+n))
done < /tmp/inf_acr_001_files.txt; echo "TOTAL=$t"        # 期望 TOTAL=9920，42 行

# ── B1：call_once 丢失失败状态
sed -n '101,104p;166,173p' lib/infrastructure/acr/backends/cuda/cuda_backend.cu
sed -n '46,51p'          lib/infrastructure/acr/backends/cuda/cuda_backend.hpp   # 契约原文

# ── B2：sanitizer 失败集漏 SKIPPED / FAILED_TO_START，exe 缺失时零记录
sed -n '10,11p;54,58p;100,103p;127,128p;156,157p;191,197p' \
  lib/infrastructure/acr/tests/sanitizer/run_acr_sanitizers.ps1

# ── B3：永久 Fail 被兜底清尾推翻（1654 起为清尾循环，1671 为入口）
sed -n '1259,1264p;1281,1289p;1394,1400p;1536,1540p;1654,1665p;1671,1713p;1720,1722p' \
  lib/infrastructure/acr/scheduler/dispatcher.cpp

# ── B4：私建线程池；对照另两条路径走 parallel_batch
sed -n '355,358p;476,479p;1136,1141p;1158p;1181,1187p;1665p' \
  lib/infrastructure/acr/scheduler/dispatcher.cpp
grep -rn "hardware_concurrency" lib/infrastructure/acr | sed 's/^/  /'

# ── B5：四个工具目录路径写错 + 配置摘要仍打印 ON
sed -n '226,253p' lib/infrastructure/acr/CMakeLists.txt | grep -n "eng/tools\|Benchmark\|Tools\|Classic"
for d in eng/tools/acr_benchmark eng/tools/acr_status eng/tools/acr_report \
         eng/tools/acr_classic_runner tools/acr_benchmark tools/acr_status \
         tools/acr_report tools/acr_classic_runner; do
  [ -f "lib/infrastructure/acr/$d/CMakeLists.txt" ] && echo "EXISTS  $d" || echo "MISSING $d"
done

# ── B6：legacy_parallel 后无条件写 all_done / done_blocks / items_done
sed -n '2388,2398p;2433,2443p;2596,2606p' lib/infrastructure/acr/scheduler/dispatcher.cpp
git -c core.quotepath=false grep -n "legacy_parallel" -- lib/infrastructure/acr | sed 's/^/  /'

# ── B7：pinned_used 构造性恒不超过 limit；sample() 从不设 pinned_exceeded
sed -n '100,111p;191,211p' lib/infrastructure/acr/utilization/memory_budget.cpp

# ── 悬空引用：机器验收脚本 / 规范文档 / operation_profile schema / 日志目录
for p in lib/infrastructure/acr/ci/check_acr_dormant.py \
         eng/cmake/toolchain/verify_toolchain.py \
         lib/infrastructure/acr/schemas/operation_profile.schema.json \
         run/logs; do [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done
git -c core.quotepath=false ls-files lib/infrastructure/acr/schemas/
# 反证：以下确实存在，用于证明我排除了假阳性
for p in lib/infrastructure/acr/docs/dependency-lock.json \
         lib/infrastructure/acr/core/task_descriptor.cpp \
         eng/cmake/install_layout.cmake CMakePresets.json; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done

# ── 恒真门：被检函数收 const&（须修 #14）+ 生产侧不可达守卫
sed -n '80,86p'  lib/infrastructure/acr/examples/weighted_integration/route_profile_calibration.hpp
sed -n '317,331p' lib/infrastructure/acr/examples/weighted_integration/route_profile_calibration.cpp
sed -n '118,127p;145,148p' lib/infrastructure/acr/tests/unit/test_route_calibration.cpp

# ── 自洽式断言：期望值由被测模型自身生成；空 holdout 判 PASS；误差写进时间字段
sed -n '63,93p;110,126p;186,193p;205,212p' lib/infrastructure/acr/tests/classic/e17_model_fit.cpp

# ── 数值稳定性：负数/inf → UB；无锁读；计时失败读作 0.000 ms
sed -n '95,112p;499,513p;552,562p;577,582p' lib/infrastructure/acr/profile/profile_reader.cpp
sed -n '140,151p' lib/infrastructure/acr/backends/cuda/cuda_buffer.hpp

# ── ADR 与实现相悖（三处）
grep -n "hardware_concurrency\|未采用" lib/infrastructure/acr/docs/ADR-003-hwloc.md | sed 's/^/  /'
grep -n "__builtin_cpu_supports\|唯一来源\|MSVC 不支持" lib/infrastructure/acr/docs/ADR-004-cpu_features.md | sed 's/^/  /'
grep -n "所有第三方库\|MSYS2 pacman\|未采用" lib/infrastructure/acr/docs/ADR-008-cmake-fetchcontent.md | sed 's/^/  /'
sed -n '214,223p' lib/infrastructure/acr/CMakeLists.txt

# ── evidence manifest schema：分支 const / 无 minItems / enum 缺状态 / 零消费者
sed -n '24,26p;41,47p;48,52p;73,92p' lib/infrastructure/acr/schemas/evidence_manifest.schema.json
git -c core.quotepath=false grep -rn "evidence_manifest" -- lib/ eng/ docs/ || echo "  零消费者（已证）"
git -c core.quotepath=false rev-parse --abbrev-ref HEAD    # 期望 main，与 schema 的 const 不符

# ── Dispatcher 不在 ASan 覆盖内（须修 #24）
sed -n '20,30p' lib/infrastructure/acr/tests/sanitizer/CMakeLists.txt

# ── 假阳性排除：库目标全部有定义
grep -rhoE 'add_library\([A-Za-z_0-9]+' lib/infrastructure/acr --include=CMakeLists.txt \
  | sed 's/add_library(//' | sort -u | tr '\n' ' '; echo
```

---

## 9. UNRESOLVED（明确缺什么才能定性）

1. `scheduler/partitioner.cpp`（INF-acr-002）— `CoverageBitmap::mark_done` 重复调用时 `done_count_` 是否与位图一致。
2. `examples/weighted_integration/weighted_integration_common.hpp/.cpp`（INF-acr-002）— `integrate_one_pixel` 求和顺序/精度与 `generate_synthetic` 数据分布；`test_weighted_integration.cpp` 的全部容差建立其上。
3. `api/event.cpp`（他片）— `Event::status()` 是否持锁读 `error_code`，决定 `runtime_internal.h:23` 的严重级。
4. `include/astro/compute/acr.hpp` 的 `KernelInvocation::domain`（他片）— 是否自校验 `begin ≤ end`，决定 `dispatcher.cpp:2252` `end-begin` 回绕的严重级。
5. `qualification/benchmarks/CMakeLists.txt`（他片）— `benchmark` 是否被拉取，决定 `transfer_benchmark.cpp:22` 能否构建。
6. `tests/classic/CMakeLists.txt`（他片）— `RESOURCE_LOCK gpu0` 是否覆盖 E10Mandelbrot。
7. **需前台一次 configure**（本轮禁编译）：`tests/unit/CMakeLists.txt:10` 在 `:74 include(GoogleTest)` 之前调用 `gtest_discover_tests` 是否报 Unknown command。
8. **需前台一次 MSVC 构建**：`tests/sanitizer/CMakeLists.txt` 的 `acr_sanitizer_msvc` 无 `target_link_libraries` 且无 `ACR_WITH_BRIDGE_LOADER`，是否 LNK 失败。

---

**本轮纪律自证**：零 git 写（未 add/commit/checkout/reset/stash/`git rm --cached`）；零编译、零 ctest、零 pytest、零构建、零二进制执行；未修改任何仓内文件（唯一写入即本交付件）；未读 `/tmp/acsd_g08/`；每条结论均给 `文件:行` 或可复跑命令；无法确证者一律标 UNRESOLVED 而非编造。
