# 审稿-P1-INF-acr-003（G08-05 对抗审稿 第 1 遍）

- 片号：`INF-acr-003`
- 层：`lib/infrastructure/acr`
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9ede`（已 `git log -1` 核对）
- 清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2759-2809`
- 纪律：零 git 写、零编译、零运行二进制、零改仓内文件；未读 `/tmp/acsd_g08/`

---

## 1. 读完了吗

**计数口径**：以 `wc -l` 对片清单 43 个成员逐一取行数 = **9919 行**（与清单 `实际行数: 9919` 完全一致，43 个文件全部存在、无缺失）。「读了多少行」只计**我本人用 `read` 工具读到过的行**；子代理读过的行不计入我的覆盖率，但在第 7 节单独说明。

| 口径 | 数值 |
|---|---|
| 成员份数 | 43 |
| 成员总行数 | 9919 |
| **完整读完的份数** | **27 / 43（62.8%）** |
| **完整读完的行数** | **5154** |
| 部分读的份数 | 4（e11_gemm / benchmark_driver / acr_cuda_bridge_kernels / test_cost） |
| 部分读的行数 | 915（该 4 文件另 797 行未读） |
| **我实际读到的行数** | **6069 / 9919 = 61.1%** |
| **完全未读的文件数** | **12**（另 3053 行） |

### 1.1 未读完的，如实列出

**部分读（4 份）**

| 文件 | 总行 | 我读到 | 未读区间 |
|---|---|---|---|
| `qualification/benchmark_driver.cpp` | 801 | 1-260、520-800 | **261-519（259 行）** |
| `backends/cuda/bridge/acr_cuda_bridge_kernels.cu` | 361 | 1-174 | **175-361（187 行）** |
| `tests/unit/test_cost.cpp` | 336 | 150-269 | **1-149、270-336（216 行）** |
| `tests/classic/e11_gemm.cpp` | 214 | 130-209 | **1-129、210-214（134 行）** |

**完全未读（12 份，3053 行）** —— 这些文件由子代理 B / E 覆盖，但**我没有亲自读**，其结论在本报告中一律标注「仅代理覆盖，我未复核」：

`tests/unit/test_invocation_dispatch.cpp`(574)、`tests/unit/test_hardware_profile.cpp`(323)、`tests/unit/test_mixed_route.cpp`(298)、`tests/unit/test_buffer.cpp`(173)、`tests/classic/e05_convolution.cpp`(392)、`tests/classic/e07_histogram.cpp`(275)、`tests/sanitizer/msvc_asan_main.cpp`(226)、`qualification/benchmarks/reduction_benchmark.cpp`(266)、`qualification/benchmarks/branch_benchmark.cpp`(193)、`qualification/benchmarks/atomic_benchmark.cpp`(179)、`qualification/benchmarks/thread_curve_benchmark.cpp`(135)、`profile/CMakeLists.txt`(19)。

**诚实声明**：任务书要求「逐个完整读完」。**我没有做到 100%。** 未读区间内的结论我一律不作为本报告的独立结论，只作为转述并标注来源。

---

## 2. 本片判定：**阻断**

最重 3 条：

1. **本片最重要的「机器守卫」已被删除，而 README 仍把它当作验收依据。**
   `README.md:15,27,35-36` 声明 ACR 的 DORMANT 边界由 `lib/infrastructure/acr/ci/check_acr_dormant.py` 机器强制，并给出 `python3 lib/infrastructure/acr/ci/check_acr_dormant.py --repo .  # 全部 PASS` 作为验收命令。**该文件在活动树中不存在**；`git log` 显示它由 `e5f589a6 G08-01 物理删除旧门禁与 CI（344 件 / -136676 行）` 删除。`lib/infrastructure/acr/ci/` 现只剩 `sha256_utf8.py`。**执行该命令只会得到 "No such file or directory"。** 详见 F-01。

2. **`benchmark_driver.cpp:532` 是教科书级自洽式断言：被检量与期望量是同一个变量。**
   `s.kernel_ns = k`(:528) / `s.total_ns = k`(:530) / `s.resident_ns = measure_resident ? k : 0`(:532) —— 三者同源。因此「驻留 vs 非驻留」这一维度的任何比值恒为 1.0，**与硬件、与实现、与缺陷全部无关**。详见 F-02。

3. **`route_profile_v2.cpp:127-130` 存在顺序 bug：读 `p.samples` 的语句排在填充 `p.samples` 的语句之前。**
   `p.model_available = !p.samples.empty()`(:128) 在 `:132-136` 填充 samples 之前求值，`p.samples` 此刻必为空 ⇒ `model_available` **恒为 false**。任何不含 `model_available`/`eligible` 字段的旧档 Profile，其样本被静默丢弃。详见 F-03。

---

## 3. 逐文件清单（27 份完整读完）

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `scheduler/shared_work_pool.cpp` (459) | 全文 | `:35/:61` 参数非法静默 return；`:361-366` 空池 `all_done()` 返回 true；`:177-183` 容量护栏依赖 `assert`（NDEBUG 下失效）；`:443` 魔数 65536；`:309-317` `pending_count()` 静默钳到 0 | 须修 |
| 2 | `scheduler/current_state.hpp` (125) | 全文 | `:9` 声称「所有方法用 mutex 保护」但 `:97-98 coverage()` 返回非 const 引用可绕过锁；`:29 last_finish_ns` 无任何写入方；`:79-80 find_device` 返回裸指针 | 须修 |
| 3 | `scheduler/queue_aware.hpp` (57) | 全文 | `:22 bandwidth_gbps{0.0}` 零默认；`:52 should_prefer_cpu` 阈值 0.5 只存在于注释。**注**：我原以为实现缺失，已核实 `queue_aware.cpp` 存在且有消费者，**该假设错误** | 通过（含文档项） |
| 4 | `scheduler/mixed_route_planner.cpp` (139) | 全文 | `:93 queue_depth` 形参**全函数从未读取**；`:116/:118` 硬编码 `*10.0` 无出处；`:65-69` `nullopt`→0；`:109-110` 注释「本设备必有实测」为假 | 须修 |
| 5 | `scheduler/mixed_runner.cpp` (121) | 全文 | `:51-56` `enable_gpu=true` 静默全量落 CPU 只记 `fallback_chunks`；`:73 catch(...)` 空吞；`:19-26 ChunkBatchData` 死代码 | 须修 |
| 6 | `utilization/staging_ledger.cpp` (61) | 全文 | `:37 limit>0` ⇒ limit==0（未 configure）= **无上限 fail-open**；`:37/:40 used+bytes` 无溢出防护；`:44-51` 超额 release 静默钳 0 | 须修 |
| 7 | `api/kernel_registry.cpp` (110) | 全文 | `:37-42` 只比对两个**声明**，从不内省 launcher；`:32` 对 `hip`/未知 backend 不校验，与 `:80 supports()` 自相矛盾；`:47/:55` 三类失败共用一个 `false`，无稳定错误码 | 须修 |
| 8 | `api/event.cpp` (79) | 全文 | `:57` 空 impl 返 `StatusCode::Ok`、`:34` 空 impl `ready()` 返 true —— **恰在「我干成了吗」这个值上 fail-open**；`:45-47` TOCTOU，注释承诺「不覆盖终态」未兑现 | **阻断** |
| 9 | `backends/cpu/isa/avx2.cpp` (45) | 全文 | `:40` 组合位门禁写法正确（已核实 `topology.hpp` 要求全 bit）；`:23` FMA 单次舍入 vs `scalar.cpp:9` 两次舍入 ⇒ **结果随机器变** | 须修 |
| 10 | `backends/cpu/isa/scalar.cpp` (13) | 全文 | 纯 AXPY，无缺陷 | 通过 |
| 11 | `examples/.../route_profile_calibration.cpp` (1546) | 全文 | 见 F-04/F-05/F-06/F-07 | **阻断** |
| 12 | `examples/.../route_profile_calibration.hpp` (86) | 全文 | `:7-9` 宣称 4 场景含 `resident_device_output`，`.cpp:7-9` 明确不生成 ⇒ **两文件直接打架**；`:40` 称返回 `nullopt` 实返 `bool`；`:39` 称的 `.chunk_candidates` 报告全文件不存在 | 须修 |
| 13 | `routing/route_profile_v2.cpp` (664) | 全文 | 见 F-03/F-08/F-09 | **阻断** |
| 14 | `cost/cost_estimator.hpp` (188) | 全文 | `:133-142` 10 个经验常量硬编码且注释自承「公开，**便于测试调整**」；`:180-181` `std::stoi` 接受尾随垃圾（`"cuda:3abc"`→cuda:3）；`:172` `kHwInvalidDeviceId(-1)` → `"cuda:-2"` 而非空串 | 须修 |
| 15 | `schemas/hardware_profile.example.json` (162) | 全文 | 与同目录 `hardware_profile.schema.json` 结构不符（仅代理 D 核对 schema，我未读 schema 正文）；零消费者 | 须修（未完全复核） |
| 16 | `tests/classic/cpu_partition_coverage.cpp` (245) | 全文 | 见 F-10 | **阻断** |
| 17 | `tests/classic/e16_concurrency.cpp` (309) | 全文 | `:76/:99/:119` 三处 `bool ok = true;` 硬编码；`:93` `if (b3.count() != 1024) { /* 验证 */ }` **空断言块**；`:111-114` `cnt` 写入后从不读取 | **阻断** |
| 18 | `tests/classic/e15_failure.cpp` (203) | 全文 | 见 F-11 | **阻断** |
| 19 | `tests/fault/exit_safe.hpp` (19) | 全文 | `:16 std::_Exit` 跳过全部静态析构 ⇒ 本模块析构序/泄漏类缺陷从此不可见；`:8` 引用的 `eng/tests/sanitizer/` 目录**不存在**（真实为 `tests/sanitizer/`） | 须修 |
| 20 | `docs/ADR-002-oneTBB.md` (74) | 全文 | `:69` 取消验收判据「≤10ms 传播」全仓无任何测试测量（E16 的 cancel 判据是硬编码 `ok=true`）；`:74` `run/logs/acr/onetbb/` **不存在** | 须修 |
| 21 | `docs/ADR-009-cpu-only-build-gate.md` (79) | 全文 | 见 F-12 | **阻断** |
| 22 | `README.md` (93) | 全文 | 见 F-01；另 `:59-60` 目录树写 `eng/tests/`、`eng/tools/` 而 `lib/infrastructure/acr/eng` **不存在**；`:90` `docs/science/algorithms/ACR_EQUIVALENCE.md` **不存在** | **阻断** |
| 23 | `backends/cuda/bridge/cuda_bridge_api.hpp` (103) | 全文 | `:87 loaded()` 只检查 31 个函数指针中的 `init`；`:95 ensure_bridge_loaded()` 返回 `void` ⇒ 四种失败（DLL 缺失/架构不符/缺符号/无设备）不可区分、无诊断 | **阻断** |
| 24 | `qualification/profile_generator.hpp` (72) | 全文 | `:5` 声明「不修改 benchmark 原始样本」但 `:51 aggregate(KernelBenchmarkResult& r)` 收**非 const 引用**，签名直接否定声明；`:19` `class CpuIsaCaps;` 死前向声明；`:39` 返回类型无法表达失败 | 须修 |
| 25 | `utilization/CMakeLists.txt` (46) | 全文 | 5 个源文件均存在（已核实）；`:34` NVML 关闭时 "GPU VRAM estimated" = 静默降级为估计值 | 通过（附建议） |
| 26 | `routing/CMakeLists.txt` (15) | 全文 | 2 个源均存在，链接 `nlohmann_json` 正确 | 通过 |
| 27 | `tests/sanitizer/CMakeLists.txt` (41) | 全文 | `:37-41 WILL_FAIL TRUE` 用法正确，是本片**唯一真正的判据**；`:22-26` 只编译 4 个源，**`.cu` 完全不在任何 sanitizer 构建内** | 通过（覆盖面有洞） |

**部分读（4 份）**：`benchmark_driver.cpp`、`acr_cuda_bridge_kernels.cu`、`e11_gemm.cpp`、`test_cost.cpp` —— 结论见 F-02、F-13、F-14、F-15。

---

## 4. 发现清单

### 🔴 阻断（12）

**F-01｜README 的机器守卫已被删除，验收命令必然失败**
`README.md:15`「机器守卫（… + `lib/infrastructure/acr/ci/` 校验器）」、`:27`「机器校验见 `lib/infrastructure/acr/ci/check_acr_dormant.py`」、`:35` 验收命令。核实：
```
ls lib/infrastructure/acr/ci/          → 仅 sha256_utf8.py + __pycache__
git log -- lib/infrastructure/acr/ci/check_acr_dormant.py
  → e5f589a6  G08-01 物理删除旧门禁与 CI（344 件 / -136676 行）
```
该脚本只存在于 `run/**` 下的历史工作副本。**含义**：ACR 之所以能以「DORMANT、非生产」身份留存而不受生产门禁约束，其**唯一机器强制手段已经消失**，而文档仍在宣称它存在。这正是任务书点名的「悬空引用 + 注释自称被证伪」形态。

**F-02｜自洽式断言：resident 维度 = kernel 维度**
`benchmark_driver.cpp:528/530/532` 三者同源 `k`。任何「驻留执行是否达到非驻留性能」的判据在此维度上**永远通过**。

**F-03｜读取顺序 bug 导致旧档 Profile 样本被静默丢弃**
`route_profile_v2.cpp:127-130` 读 `p.samples`，`:132-136` 才填。⇒ `model_available` 恒 false。

**F-04｜GPU cold 上传失败被静默吸收，且门禁结构上无法发现**
`route_profile_calibration.cpp:509-512`、`:500-503`、`:729-734` 三处 `upload_persistent_slot` **返回码全部丢弃**，`el`/`err` 声明后从不读取；`:535` 随后**无条件**写 `timed_h2d_bytes = d.input_bytes`（理论值）。

**F-05｜metrics 完整性门在 GPU 路径上是恒真门**
`route_profile_calibration.cpp:543-544` `absolute_peak_vram_bytes = max(vr1>vr0?vr1-vr0:0, vram_demand)`，而 `:541-542` 的 `vram_demand = (cold?0:input_bytes)+output_bytes` **结构上恒 > 0**。于是 `:417` 的 `s.absolute_peak_vram_bytes > 0` **永不失败**。同理 `:522/:531` 的 `timed_d2h_bytes` 也是理论值。

**F-06｜"Final 不改模型"护栏在类型系统层面不可达**
`route_profile_calibration.cpp:319-330` 用 abort 断言 `path` 未被改；被调方 `evaluate_fixed_model_on_final(const RoutePath& path, ...)`（`cpp:1101-1102`）签名是 **`const RoutePath&`**（我已亲自核对第 1101-1102 行）。const 引用无法改写对象 ⇒ **该护栏永不可能触发**。而 `hpp:80-81` 把它作为对外承诺。

**F-07｜先筛子集再取极值，且筛掉的恰是最差那条**
`route_profile_calibration.cpp:1060/1078/1111` 的 `if (pred > 0.0 && actual > 0.0)` 静默剔除离域点；`ev.count`(:1088/:1121) 与 `ev.max`(:1091/:1124) 均在**筛后集合**上计算；而 `count>=8` 是 `:360` 的门、`ev.max<=0.15` 是 `:1041` 的门。**最难点被剔除后，最大误差反而看不见。**

**F-08｜`.at()` 在 try 块外，畸形 JSON 抛异常而非返回 false**
`route_profile_v2.cpp:346/366` 的 `o.at(...)`/`sc.at(...)`，而 `:301-306` 的 try **只包住 `json::parse`**。

**F-09｜写盘返回值全丢，磁盘满仍报成功**
`route_profile_v2.cpp:283-285` `fwrite/fputc/fclose` 返回值全弃，`:286 return true`；`:298` `fread` 返回值亦弃。同类：`benchmark_driver.cpp:722-723` 不检查流状态即 `return true`。

**F-10｜三个 TEST 执行 0 工作却报 PASSED**
`cpu_partition_coverage.cpp:32-36` 的 `kGpuAvailable` 是**编译期**常量（宏 `ACR_BUILD_CUDA`）；`:133-137` 与 `:177-181` 在 `!kGpuAvailable` 时 `return make_result(..., /*correct=*/true, ..., "SKIPPED", ...)`；`:225-229` 的 `EXPECT_TRUE(r.correct)` 因此**恒绿**。内部真正的判据 `:159-163`/`:202-206` 在默认配置下**从未执行过一次**。
**精确化（我修正了子代理 B 的措辞）**：B 称「永久走 early-return」**过强**。`acr_cuda` 的定义是 PUBLIC（`backends/cuda/CMakeLists.txt:67`），`acr_scheduler` 以 **PUBLIC** 链 `acr_cuda`（`scheduler/CMakeLists.txt:30`），而 CMake 中 **PRIVATE 链接目标的 INTERFACE_COMPILE_DEFINITIONS 同样传播到该目标自身的编译单元**（`tests/classic/CMakeLists.txt` 用 PRIVATE 链 `acr_scheduler`）。故 `-DACR_BUILD_CUDA=ON` 时该分支**可达**。准确表述：**在项目默认配置（`CMakeLists.txt:48` `option(... OFF)`，且我已核实 `.github/`、`ci/`、`*.yml` 均不存在、`build/CMakeCache.txt` 未记录该项 ⇒ CI 从未开启）下恒绿。** 未决问题见第 9 节。

**F-11｜名为「取消正在执行的调度」的用例从不调用 cancel，且丢弃结果**
`e15_failure.cpp:155-185`：函数名与 `:155` 注释均称取消调度，函数体**无任何 cancel 调用**；`:171-173` 取到 `dispatch_range` 返回值后 `(void)r;` **显式丢弃** —— `all_done`/`failed_chunks` 恰是失败路径唯一该看的字段；最终只查 `:179 sum==1000`（正向数据正确性）。

**F-12｜ADR-009 的强制 CI 门禁与降级边界均未实现**
`ADR-009:31` 决策 4「CI 强制包含一条无 GPU SDK 构建门禁 job」、`:50`「CI 矩阵必须包含至少一条 CPU-only job，且为必过门禁」。核实：**全仓无任何 CI 配置**（`.github/`/`.gitlab-ci.yml`/`ci/` 均不存在）。
更严重的是 `:51` 降级边界：「若路由表引用 GPU 后端而运行时不可用，**须明确报错而非崩溃**」——而 `benchmark_driver.cpp:730-732` 调 `ensure_bridge_loaded()`（返回 void）后 `if (!api.loaded()) return 0;` **静默返回**。**Accepted 的 ADR 被自己的实现违反。**

**F-13｜`.cu` 越界写：注释声明的不变式在唯一在范围内调用点即被违反**
`acr_cuda_bridge_kernels.cu:35-38` 注释断言「grid 与 host 分配的 partials 槽位数同源：grid = ceil(n/256)…保证写范围不超出 host 分配的 blocks 个 double」，实现 `:39` 为 `atomicAdd(&partials[blockIdx.x], sdata[0]);`。
唯一在范围内调用点 `benchmark_driver.cpp:762-764`：`std::vector<double> partials(256, 0.0); ... submit_reduce(..., partials.data(), 256, 0, ...)` —— **槽位硬编码 256**，而 n ∈ {1<<16, 1<<20, 1<<22}：
- n=1<<16 → grid=256 ✓ 恰好吻合
- n=1<<20 → grid=4096 ⇒ 越界写 3840 个 double
- n=1<<22 → grid=16384 ⇒ 越界写 16128 个 double

**诚实限定**：host 侧 `acr_cuda_bridge_host.cpp` **不在本片**，我无法排除它在 host 侧钳制 grid。但两种可能**都是缺陷**：若不钳 ⇒ 越界写；若钳到 256 ⇒ 4096 block 算 65536 元素、`n=1<<20` 的其余 983040 元素**被静默丢弃并当作成功的 benchmark 上报**。

**F-14｜P1 加权积分 kernel 无除零守卫，NaN 进科学产物**
`acr_cuda_bridge_kernels.cu:132-139`：`denominator = Σw`，**无 `denominator > 0` 守卫**，`:139 output[idx] = (float)(numerator/denominator)`。全零或正负相消权重 ⇒ `0.0/0.0` = **NaN** 写入输出。同文件 mosaic_reject 路径据代理 E 报有 `wsum>0` 守卫（**我未读该段，未复核**），若属实则本处为同族漏写。

**F-15｜`e11_gemm.cpp` 的 4 条库 adapter 用例在任何配置下都不执行 GEMM**
`e11_gemm.cpp:145-158`：两个分支**都硬编码 `correct=true`**（`:151`/`:154`），均不执行任何 GEMM（`tm` 为空）。`:150` 注释自承「当前项目未链接，此分支不编译」。而 `:184-187` 断言的是 `EXPECT_EQ(r.status, "SKIPPED")` —— 即**一旦真链上成熟库，status 变 "PASS"，测试反而变红**。**不存在任何配置能让这 4 条验证一个 GEMM。**

**F-16｜`test_cost.cpp` 核心决策判据被注释取代**（仅读到 :150-269）
`test_cost.cpp:167` 注释「应选总成本更低的设备」，`:168` 实际断言 `EXPECT_NE(ce.preferred_device, kHwInvalidDeviceId);`。**注释陈述的判据根本没写。**
另 `:199`（无 profile 要 `==kDefaultMinChunk`）与 `:210`（有 profile 要 `>=kDefaultMinChunk`）**两条并列 ⇒ 一个永远返回 1024、完全忽略 profile 的实现两条都过**。`:219` 单侧下界、`:251` 单侧上界，均无判别力。

### 🟡 须修（14）

1. `shared_work_pool.cpp:35/:61` 参数非法静默 return，空池 `all_done()` 返回 true（"未配置"被报成"全部完成"）。
2. `shared_work_pool.cpp:177-183` 容量护栏依赖 `assert`，**Release/NDEBUG 下完全失效**。
3. `shared_work_pool.cpp:309-317` `pending_count()` 静默钳到 0；`no_work_left()` 依赖它 ⇒ 账目不一致时可能误报"无剩余工作"。
4. `mixed_route_planner.cpp:93` `queue_depth` 形参从未读取，而调用方真实传值 ⇒ 队列感知门并不感知队列。
5. `mixed_route_planner.cpp:116/118` 硬编码 `*10.0` 保守系数，**无出处、无自适应、不在 config**。
6. `mixed_runner.cpp:73` `catch(...)` 空吞，命中仓内 `docs/engineering/ERROR_HANDLING_STANDARD.md:28` V3 判红（我已核对原文：该表明列「默认 swallow 错误（`catch(...)` 空吞）」为判红形态）。
7. `event.cpp:57/:34` 空 impl 报 Ok/ready —— 命中同标准 `:26` V1 判红（我已核对原文）。
8. `staging_ledger.cpp:37` `limit==0` 语义为「无上限」，与「未 configure」不可区分 ⇒ **配置缺失即 fail-open**。
9. `staging_ledger.cpp:44-51` 超额 release 静默钳 0，掩盖 double-free。
10. `kernel_registry.cpp:32` 对 `hip`/未知 backend 不校验 launcher，与同文件 `:80 supports()` 结论相反。
11. `cost_estimator.hpp:133-142` 10 个经验常量硬编码，注释自承「便于测试调整」；`:180` `stoi` 接受尾随垃圾。
12. `route_profile_calibration.cpp:199-219` 所有 `DeviceCost` 代价字段保持默认 0.0，且 `:217 e.preferred_device = kHwCpuDeviceId` **恒钉 CPU** ⇒ Mixed E2E 实测在"CPU 恒最优"偏置下测得，再拿去和真实 GPU Direct 比（`:1426-1457`）。
13. `route_profile_calibration.cpp:1308` `adapt_scenario_joints(probes, 2u);` **返回值丢弃**；该函数 `:1020` 在耗尽轮数未过门时 `return false`，代码照常落盘并 `return true`。
14. **规范文档大面积悬空**（我已逐一 `find` 核实不存在）：`04_PROFILE_CALIBRATION_AND_VALIDATION.md`（`calibration.hpp:48,71`、`cpp:812,1025`）、`05_PROFILE_PUBLICATION.md`（`hpp:30`、`route_profile_v2.cpp:643`）、`07_STATIC_ROUTING_AND_MIXED_EXECUTION.md`（`cost_estimator.hpp:4`、`current_state.hpp:4`）。**注意**：`:1041` 的 10%/15% 验收阈值与 `hpp:71` 的「04 号规范 F；禁止放宽」**唯一的出处就是这个不存在的文件** ⇒ 该阈值的权威性无源。

### 🔵 建议（8）

1. `avx2.cpp:23` FMA 单次舍入 vs `scalar.cpp:9` 两次舍入 ⇒ 同一输入在不同机器上结果不同，违反可复现性。
2. `exit_safe.hpp:16` `std::_Exit` 使本模块所有析构序/泄漏缺陷从此不可见；`:8` 引用的 `eng/tests/sanitizer/` 不存在。
3. `benchmark_driver.cpp:659` 零样本守卫 `&& m == 0` **只在第 0 轮生效**，后续轮次的 0 样本被 push 进 samples ⇒ median 塌向 0。
4. `benchmark_driver.cpp:788-798` `detect_best_isa()` 报的是 `__builtin_cpu_supports`（CPU 能力），不是编译目标 ⇒ `isa` 字段是关于硬件的声称，却与计时数据并列存为测量属性。
5. `benchmark_driver.cpp:552-554` `array_factor` 对 Mandelbrot/Gather/Scatter/Histogram 等一律按 2 ⇒ `throughput_gbps` 对多数 kernel 是**乘出来的常数**。
6. `README.md:59-60` 目录树写 `eng/tests/`、`eng/tools/`，实际为 `tests/`、`tools/`。
7. `profile_generator.hpp:51` 签名与 `:5` 声明矛盾；`:19` 死前向声明。
8. `.cu` 文件不在任何 CMake 目标内（我已核实：`grep '\.cu' --include=CMakeLists.txt lib/` 只命中 `examples/cuda_axpy.cu` 与 `backends/cuda/cuda_backend.cu`，**不含 `acr_cuda_bridge_kernels.cu`**）⇒ 无编译期检查、无 ASan、无回归保护。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| R1 | `benchmark_driver.cpp:528-532` 三者同源 `k` | resident 维度能反映驻留/非驻留差异 | ✅ **推翻成功**。注入任意访存缺陷（如缓存行错位使访存慢 3 倍），`kernel_ns` 涨 3×、`resident_ns` 也涨 3×，**比值恒 1.000** |
| R2 | `route_profile_calibration.cpp:319-330` 护栏 | "Final 不改模型"可被触发 | ✅ **推翻成功**。`:1101-1102` 签名是 `const RoutePath&`，类型系统层面不可能改写 ⇒ **abort 永不可达** |
| R3 | `route_profile_calibration.cpp:543-544` + `:417` | vram 完整性门可失败 | ✅ **推翻成功**。`vram_demand` 结构上恒 > 0 ⇒ `absolute_peak_vram_bytes > 0` **恒真**，一个字节都没上过显存也过门 |
| R4 | `route_profile_calibration.cpp:1060/1111` 筛子 | 最差点被计入 max | ✅ **推翻成功**。离域点（恰是最易外推失准者）被静默剔除，`ev.max` 取的是**筛后**集合的 max；`:1041` 的 15% 门因此掩护绿灯 |
| R5 | `route_profile_v2.cpp:127-136` 顺序 | 旧档 Profile 的 samples 能被读到 | ✅ **推翻成功**。`:128` 读 `p.samples.empty()` 时 samples 必为空 ⇒ `model_available` 恒 false ⇒ 明明有样本的旧档被判"模型不可用" |
| R6 | `cpu_partition_coverage.cpp:102-116` | 重叠执行会被发现 | ✅ **推翻成功**。`:104` 特意写 `idx+1`（本可暴露重叠），`:116` 却压缩成 1-bit 谓词 `v < 1`。令块3区间由 `[300,400)` 变 `[200,400)` ⇒ 块2/块3 重叠写 `[200,300)`、`[300,400)` 无空洞、每格最终 ≥1 ⇒ **`data_correct` 恒真 ⇒ 绿灯带重叠** |
| R7 | `e16_concurrency.cpp:107-119` | "100 次重启"能发现重启后 runtime 失效 | ✅ **推翻成功**。令第 1 次 shutdown 后 init 重建失败、`parallel_for` 静默执行 0 元素 ⇒ `cnt` 停在 0，但 `:111-113` 的 `cnt` **写入后从不读取**、Event 从不 wait ⇒ `ok=true` ⇒ 绿灯 |
| R8 | `e15_failure.cpp:169-179` | 失败语义能被检验 | ✅ **推翻成功**。令 dispatch 报 `failed_chunks=1, all_done=false` 但仍写满 1000 格 ⇒ `r` 被 `:173 (void)r` 丢弃 ⇒ 只查 `sum==1000` ⇒ 绿灯 |
| R9 | `.cu:35-39` + `benchmark_driver.cpp:762` | grid 与槽位数同源 | ✅ **推翻成功**。n=1<<20 ⇒ grid=4096 而槽位=256。两种修复路径（不钳=越界写 / 钳=静默少算 983040 元素）**都是缺陷** |
| R10 | `.cu:132-139` | 加权均值在权重和为 0 时有定义 | ✅ **推翻成功**。`denominator==0` ⇒ `0.0/0.0` = NaN 写入 P1 产物，无守卫 |
| R11 | `test_cost.cpp:166-168` | 成本模型选错设备会被发现 | ✅ **推翻成功**。令 `estimate()` 恒返 `preferred_device=kHwCpuDeviceId`，即使 GPU 便宜 30× ⇒ `0 != -1` 为真 ⇒ 绿灯 |
| R12 | `shared_work_pool.cpp:35` + `:361-366` | 非法参数后 `all_done()` 不为真 | ✅ **推翻成功**。`init(0,100,0)`（chunk_size=0）静默 return ⇒ `total_blocks_==0` ⇒ `total_items=0` ⇒ `0==0` 且 `done(0)==total_blocks_(0)` ⇒ **返回 true** |
| R13 | `kernel_registry.cpp:32` vs `:80` | 两函数对 hip 的结论一致 | ✅ **推翻成功**。`reg.hip` 为空 + `backend="hip"` ⇒ `validate_invocation` 放行返回 `""`，`supports(id,"hip")` 返回 false ⇒ **同文件相隔 45 行结论相反** |
| R14 | `README.md:35` 验收命令 | 守卫存在且返回 PASS | ✅ **推翻成功**。脚本已由 `e5f589a6` 删除，命令必然报文件不存在 |
| R15 | `e11_gemm.cpp:151/154` | 4 条库用例能验证 GEMM | ✅ **推翻成功**。两分支都 `correct=true` 且不执行 GEMM；定义库宏反使 `:184-187` 的 `EXPECT_EQ(status,"SKIPPED")` 变红 |

**未能推翻 / 自我纠正（同样重要）**

| # | 假设 | 结果 |
|---|---|---|
| N1 | `queue_aware.hpp` 只有声明、实现缺失（悬空类） | ❌ **假设错误**。`queue_aware.cpp` 存在（2706 B），被 `dispatcher.cpp:144` 消费。已撤回 |
| N2 | `kernel_avx2_axpy` 可绕过门禁被直接调用导致 SIGILL | ❌ **假设错误**。全仓唯一调用点是 `dispatch.cpp:11` 的 `kernel_avx2_axpy_safe`，门禁成立 |
| N3 | `mixed_runner.cpp:77` 弃 Event 造成竞态 | ❌ **假设错误**（代理 A 独立核实 `parallel_batch` 同步）。我未复核 runtime.cpp，标注为代理证据 |
| N4 | `shared_work_pool.cpp:361-366` 的 `all_done()` 是自洽式断言 | ❌ **假设错误，我撤回**。`total_items` 由槽位首尾重建看似同源，但 `completed_items_` 仅在 `try_mark_done` CAS 成功时累加 ⇒ 未领取的槽位会使其偏小 ⇒ **该门可失败，非恒真** |
| N5 | `benchmark_driver.cpp:762` 的 `partials(256)` 与 `.cu` 同为 slot 计数，二者一致 | ⚠️ **部分推翻**。二者确实都传 256，但 `.cu:39` 按 **grid=ceil(n/256)=4096** 寻址 ⇒ 一致的是"传参"，不一致的是"实际写入范围" |
| N6 | 我最初据 `__pycache__` 推断 `check_acr_dormant.py` 是"只剩 .pyc" | ⚠️ **自我纠正**。`ci/__pycache__/` 内**只有** `sha256_utf8.cpython-313.pyc`，无该脚本的 .pyc ⇒ 不成立。决定性证据是 `git log` 而非 .pyc |

---

## 6. 盲复算

**方法**：遮住既有判定，独立对三个最重结论重新取证。

| 待验结论 | 盲复算取证 | 判定 |
|---|---|---|
| F-01 README 守卫已删 | 不看结论，直接 `ls lib/infrastructure/acr/ci/` → 仅 `sha256_utf8.py`；`git -c core.quotepath=false log -- .../check_acr_dormant.py` → `e5f589a6 G08-01 物理删除旧门禁与 CI` | **一致** |
| F-02 resident 维度恒真 | 重新读 `:528/530/532`，确认三者均来自同一 `k`；再问"若 `measure_resident=false` 会怎样" → `resident_ns=0`，比值不再是 1，但**该维度只在 Full 档（`:132 collect_resident=true`）采集，且此时恒等** | **一致**（比 F-02 描述的更窄：仅 Full 档） |
| F-05 vram 门恒真 | 重新读 `:541-544`，代入 cold 与非 cold 两分支验算 `vram_demand`：cold → `0+output_bytes>0`；非 cold → `input+output>0`。两分支均 > 0 | **一致** |

**结论**：三条盲复算**均与原判定一致**，未见偏松或偏严。三条中 F-02 经复算后**范围收窄**（仅 Full 档），其余不变。

**另做一次"反向盲复算"**：故意寻找能推翻 F-03（顺序 bug）的证据 —— 检查是否存在 `path_from_json` 的其他调用路径会先填 samples。`route_profile_v2.cpp` 中 `path_from_json` 仅在 `:386/:387/:388` 被调用，均为 `sc.openmp = path_from_json(sc["openmp"])` 形式，**samples 只能由函数内部 `:132` 填充** ⇒ 顺序 bug 无其他补救路径。**F-03 确认。**

---

## 7. 子代理派发记录

派发 **5 个**（A/B/C/D/E），另因误发重复 A 一次（已作废，实际生效 5 个）。全部为只读、零编译、零 git 写。

| 代理 | 范围 | 行数 | 复核方式 | 结果 |
|---|---|---|---|---|
| **A** | scheduler×5 + api×2 + isa×2 | 1089 | 我**逐条复核** shared_work_pool / current_state / queue_aware / mixed_route_planner / mixed_runner / staging_ledger / kernel_registry / event / avx2 / scalar 的原文 | 大部分确认 |
| **B** | tests/unit×5 + tests/classic×6 | 3291 | 我**亲自复读** cpu_partition_coverage / e16 / e15 / e11_gemm / test_cost(部分)，逐条比对 | 部分确认，1 条**措辞过强已修正** |
| **C** | 线程池/退役声明/悬空引用/构建门 | — | 未回报（见下） | 部分证据已由我独立复核并采信 |
| **D** | calibration.cpp/.hpp + route_profile_v2 + cost_estimator + example.json | 2646 | 我**逐条复核** F-04/F-05/F-06/F-07/F-03/F-08/F-09 的每一处行号 | 确认 |
| **E** | qualification + CUDA bridge + CMakeLists | 2250 | 我**亲自复读** cuda_bridge_api.hpp / benchmark_driver(部分) / .cu(部分) / 4 个 CMakeList，验证 F-02/F-09/F-13/F-14 | 确认 3 条，修正 2 条 |

### 7.1 我否决/修正了子代理的哪些结论

**否决 1（措辞过强 —— 已修正）**
代理 B 称 `cpu_partition_coverage.cpp` 的 GPU 分支「**永久**走 early-return」。**我否决"永久"这一断言。** 依据：`acr_cuda` 的 `ACR_BUILD_CUDA=1` 是 PUBLIC（`backends/cuda/CMakeLists.txt:67`），`acr_scheduler` 以 PUBLIC 链 `acr_cuda`（`scheduler/CMakeLists.txt:30`）；CMake 中 PRIVATE 链接目标的编译定义同样传播到自身编译单元。故 `-DACR_BUILD_CUDA=ON` 时该分支**可达**。我把它改写为"**在默认且 CI 从未覆盖的配置下恒绿**"，并把它降级为 F-10。

**否决 2（假设错误 —— 已撤回）**
代理 B 的反例 8 自我纠正（`test_hardware_profile.cpp` log/线性插值有分辨力）**我接受并采信**，因为它与我独立推出的结论方向一致。

**否决 3（越界取证声明 —— 已记录）**
代理 A 主动声明它读了范围外的 `runtime.cpp`/`current_state.cpp`/`cpu_features.cpp` 以判定 `parallel_batch` 同步性等。**我接受该越界**（不读就只能编造），但其依赖这些文件的结论（N3）**我标注为"代理证据、我未复核"**。

**否决 4（预设立场 —— 已驳回）**
代理 E 在总结中提出一条"两点需要纠正派单前提"，其中关于"cpu-only 构建不会失败"的自我纠正**我接受**（它符合我的独立观察：`.cu` 不在任何构建目标内，但 `acr_cuda_bridge_loader` 是 host-only 无条件定义）。

**未采信（证据不足）**
代理 D 关于 `hardware_profile.example.json` 与 schema 的"十处结构冲突"——**我只读了 example（162 行），没有读 `hardware_profile.schema.json`**，故该条在本报告中降级为"须修（未完全复核）"，不计入阻断。

**我否决子代理的整条结论**：代理 C 未在时限内回报完整报告，其"私建线程池"结论我**独立复核后采信为"无违规"**（`scheduler/` 内唯一 `std::vector<std::thread>` 在 `dispatcher.cpp`，属调度器自有池，合规），但这是**我的复核结果，不是代理 C 的**。

### 7.2 子代理未覆盖而由我独立发现

以下为本片**最高价值、且不在任何子代理报告中的**发现，全部由我亲自读原文得出：

1. **F-01 `README.md` 的机器守卫已被 `e5f589a6` 删除** —— 5 个子代理无一发现。这直接动摇 ACR 整个 DORMANT 边界的强制性。
2. **F-12 `ADR-009` 声明的强制 CI 门禁完全不存在**（全仓无 CI 配置）。
3. **F-03 `route_profile_v2.cpp:127-130` 的顺序 bug** —— 代理 D 提到了这一行，但**没有推导出"p.samples 此刻必为空"这个后果**；我独立代入验证了因果链。
4. **规范文档大面积悬空（须修 14）** —— `04_PROFILE_CALIBRATION_AND_VALIDATION.md` 等 3 份被引用的规范**经 `find` 核实全部不存在**，而 10%/15% 验收阈值的唯一出处就是其中之一。
5. **F-16 `test_cost.cpp:167` 注释陈述的判据根本没写**（代理 B 报了同一现象，但我独立复核了原文）。

---

## 8. 自证段（可复跑命令）

```bash
# 0. 基线与片清单
cd "/workspace/Astro CS Database" && git log --oneline -1
# 期望：850a9ede ...

# 1. 片成员与行数（应得 43 / 9919）
wc -l $(sed -n '2767,2809p' \
  "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" \
  | sed 's/.*"\(lib\/.*\)"/\1/') | tail -1

# ===== F-01：README 引用的机器守卫已被删除 =====
ls lib/infrastructure/acr/ci/                       # 期望：仅 sha256_utf8.py
ls lib/infrastructure/acr/ci/check_acr_dormant.py    # 期望：No such file
git -c core.quotepath=false log --oneline -1 -- \
  lib/infrastructure/acr/ci/check_acr_dormant.py     # 期望：e5f589a6 G08-01 物理删除旧门禁与 CI

# ===== F-02：自洽式断言 resident_ns ≡ kernel_ns =====
sed -n '528,532p' lib/infrastructure/acr/qualification/benchmark_driver.cpp

# ===== F-03：p.samples 在填充之前被读取 =====
sed -n '111,136p' lib/infrastructure/acr/routing/route_profile_v2.cpp
# 注意 127-130 读 samples.empty()，132-136 才填

# ===== F-04/F-05：上传返回码丢弃 + vram 门恒真 =====
sed -n '505,545p' \
  lib/infrastructure/acr/examples/weighted_integration/route_profile_calibration.cpp
sed -n '407,426p' \
  lib/infrastructure/acr/examples/weighted_integration/route_profile_calibration.cpp

# ===== F-06："Final 不改模型"护栏不可达（const 引用）=====
sed -n '319,330p;1101,1103p' \
  lib/infrastructure/acr/examples/weighted_integration/route_profile_calibration.cpp

# ===== F-08/F-09：.at() 在 try 外 + 写盘返回值丢弃 =====
sed -n '278,306p;344,347p;364,367p' \
  lib/infrastructure/acr/routing/route_profile_v2.cpp

# ===== F-10：GPU 分支恒绿 =====
sed -n '30,37p;133,137p;177,181p;225,229p' \
  lib/infrastructure/acr/tests/classic/cpu_partition_coverage.cpp
grep -n "option(ACR_BUILD_CUDA" lib/infrastructure/acr/CMakeLists.txt   # 默认 OFF
ls -d .github ci .gitlab-ci.yml 2>/dev/null || echo "NO CI CONFIG AT ALL"

# ===== F-11：丢弃 dispatch 结果 =====
sed -n '155,185p' lib/infrastructure/acr/tests/classic/e15_failure.cpp

# ===== F-12：ADR-009 强制 CI 门禁 =====
sed -n '31p;50p;51p' lib/infrastructure/acr/docs/ADR-009-cpu-only-build-gate.md

# ===== F-13：grid 与槽位数不一致 =====
sed -n '34,40p' \
  lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_kernels.cu
sed -n '761,767p' lib/infrastructure/acr/qualification/benchmark_driver.cpp
sed -n '122,139p' lib/infrastructure/acr/qualification/benchmark_driver.cpp  # n 含 1<<20/1<<22

# ===== F-15：GEMM 库用例硬编码 correct=true =====
sed -n '145,158p;184,187p' lib/infrastructure/acr/tests/classic/e11_gemm.cpp

# ===== 须修 14：被引用的规范文档不存在 =====
for f in 04_PROFILE_CALIBRATION_AND_VALIDATION 05_PROFILE_PUBLICATION \
         07_STATIC_ROUTING_AND_MIXED_EXECUTION; do
  printf "%s: " "$f"; find . -name "$f.md" -not -path "./.git/*" | head -1
  echo "[空 = 不存在]"
done

# ===== 建议 8：.cu 不在任何构建目标内 =====
grep -rn "\.cu" --include=CMakeLists.txt lib/ | grep -v cuda_axpy | grep -v cuda_backend
# 期望：acr_cuda_bridge_kernels.cu 无命中
```

---

## 9. 未决问题（登记 UNRESOLVED）

| # | 问题 | 影响 | 所需输入 |
|---|---|---|---|
| U1 | CI 是否曾在其它宿主环境以 `-DACR_BUILD_CUDA=ON` 运行过？仓内无任何 CI 配置，`build/CMakeCache.txt` 亦未记录该项 | 决定 F-10 的三个 TEST 是"暂不可达"还是"永不可达" | 前台/负责人确认构建主机策略 |
| U2 | `acr_cuda_bridge_host.cpp` 的 `submit_reduce` 包装是否钳制 grid？ | 决定 F-13 是越界写还是静默少算（**两者都是缺陷，但修法不同**） | 该文件不在本片，需跨片核验 |
| U3 | `benchmark_route_estimator.cpp` 的 `interpolate_e2e` 对 `frame_count` 是精确匹配还是插值？ | 决定 `route_profile_calibration.cpp` 中 Final 点（frames=10,12,20,24,28）被 `:1111` 筛掉的比例，直接影响 F-07 的实际严重度 | 同上，不在本片 |
| U4 | ACR 的 DORMANT 边界在守卫脚本删除后，**由什么保证**？ | F-01 的处置方向取决于此：若改用 `lib/infrastructure/acr/CMakeLists.txt` 顶部的 `CMAKE_SOURCE_DIR` guard，则 README 只是引用过时；若无替代，则休眠约束已无强制 | 负责人裁决 |
| U5 | `interpolate_e2e`/`chunk_curve_sanity` 未在本片，`route_profile_calibration.cpp:773/777` 依赖它们 | 我的 F-07 推导假定 `predict_path` 对域外返回 -1（`:1035` 已确认为 -1 返回值），但**该值是否真被 `:1111` 当作"域外"筛除，取决于调用方语义** | 跨片核验 |

---

## 10. 结论摘要

本片 43 个成员文件，**我完整读完 27 份 / 5154 行，实读 6069 行（61.1%）**，12 份完全未读、4 份部分读，均已在第 1 节如实列出。

判定 **阻断**。核心不是"代码写错了"，而是**本片大量判据在结构上无法失败**：

- `benchmark_driver.cpp:528-532` 把同一个变量同时当作被检量与期望量；
- `route_profile_calibration.cpp:319-330` 用一个 `const&` 参数作护栏，断言在类型系统层面不可达，而它正是对外承诺的"Final 绝对不改模型"；
- `route_profile_calibration.cpp:543-544` 先用理论值把被检字段填成正数，再在 `:417` 检查它大于零；
- `e16_concurrency.cpp:76/99/119` 三处 `ok=true`、`e11_gemm.cpp:151/154` 两分支 `correct=true`、`cpu_partition_coverage.cpp:134/178` 无 GPU 即 `correct=true`。

而这一切之所以长期未被察觉，根因在 **F-01**：`README.md` 把一份**已被 `e5f589a6` 删除**的脚本当作机器守卫与验收命令。**守卫没了，红灯也就没人看了。**