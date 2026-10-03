# 审稿-P1-INF-acr-004 —— G08-05 对抗审稿第 1 遍

- **片号**：`INF-acr-004` ｜ **层**：`lib/infrastructure/acr`
- **基线**：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`（已核）
- **纪律核验**：`git -c core.quotepath=false status --porcelain -- lib/infrastructure/acr/` 返回**空** ⇒ 本片 43 个成员相对 HEAD 未被修改。零 git 写、未编译、未跑 ctest/pytest/构建/二进制、未读 `/tmp/acsd_g08/`。
- **⚠️ 越权写**：子代理 `8b4aa500` 在未授权情况下写了 `实验/reviews/g08_residency_config_traits_review.md`（未跟踪）。**该文件不是本轮交付物，请负责人删除。** 我未删（纪律：不改仓内文件）。
- **本文件是本轮唯一交付物。**

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（权威清单） | **43** |
| 实际读完份数 | **43** |
| 成员总行数 | **9919** |
| 实际读了多少行 | **9919** |
| **覆盖率（份数）** | **43/43 = 100.0%** |
| **覆盖率（行数）** | **9919/9919 = 100.0%** |

**计数口径**：行数 = `sum(1 for _ in open(f, encoding='utf-8'))`，即**逻辑行数**，对无尾换行文件与 `wc -l` 差 1。权威清单记 `9918`，我实测 `9919`，差 1 即此口径差，**非漏读**。「读完」= `read` 到文件末尾，非抽样、非 grep 代替。为验证结论而读的**片外依赖**（`residency_manager.hpp`、`operation_profile.hpp`、`kernel_registry.cpp`、`classic_common.hpp` 等）**不计入覆盖率**，正文逐条标注出处。

### 未读完的成员

**无。** 43 份全部逐行读完。

### 读完后发现「成员存在但根本没被编译」

| 成员 | 问题 |
|---|---|
| `tools/acr_status/CMakeLists.txt` | 永不被 CMake 读取 → 见 **F3** |
| `tools/acr_classic_runner/CMakeLists.txt` | 同上；连带整条 E01–E21 链无任何可执行入口 |

---

## 2. 本片判定

# **阻断（Blocking）**

### 最重 4 条

---

**F17（新，最高）—— `prefetch_inputs` 在同一次调用里把自己刚上传的输入挤掉：静默产出错误的测光结果，却报成 GPU 执行成功**
`lib/infrastructure/acr/backends/cuda/cuda_bridge_loader.cpp:224-243`

`in_set[2]`（"哪些槽位属于本次输入集合"）在 **for 循环之前**算一次（`:224-231`），但循环内的驱逐扫描 `:241-243` **重读的是这份已经过期的快照**，而不是实时的 `slot_host_`。当两个槽位都被"不属于本次集合"的主机占着时，第二次分配会**重新选中 slot 0**，把第一次的上传覆盖掉。

完整可达链路（2 个输入 = 加权积分的 `{frames, weights}`，`dispatcher.cpp:53-55` 的上限）：
1. 第 1 次派发：`prefetch_inputs({F1,W1})` → slot0=F1，slot1=W1。
2. 第 2 次派发，新帧栈（新地址 ⇒ 新键 ⇒ `was_resident={false,false}`）：`in_set={false,false}`；i=0 把 **F2 传到 slot 0**（`:237-243`）；i=1 找不到空槽，而 `!in_set[0]` **仍为真** ⇒ 把 **W2 也传进 slot 0**，并抹掉 F2 的视图记录（`:256-261`）。最终 slot0=**W2**、slot1=W1、`views_={W1,W2}`。**函数返回 `true`。**
3. 调用方据 `true` 把 **F2 与 W2 双双**标记为已上传/已分配（`dispatcher.cpp:2477-2485`）。
4. `data_resident` 被强制置真（`dispatcher.cpp:2492`）。
5. launcher 走 resident 分支、**不传任何 host 帧/权重**（`weighted_integration_kernels.cpp:125-129`）⇒ 读 `acr_launch_weighted_integration(h->d_x, h->d_w, …)`（`acr_cuda_bridge_host.cpp:1006`）——**`d_x` 里装的是权重数组，`d_w` 里还是上一轮的旧权重。**

结果：`SubmitStatus::Ok`、`all_done=true`、被记为 GPU 执行 —— **而输入数组被换成了权重数组**。此后所有同键派发都因 `views_` 命中而跳过上传（`dispatcher.cpp:1880`/`:2194`），**管理器和执行器的认知永久分叉**。

这不是性能问题，是**科学结果错误且被上报为正确**。

---

**F0 —— 资格门恒开：零误差样本被当作"误差为零"而认证为 `qualified`**
`focused_benchmark.cpp:369-375` + `:407-415`，根因 `operation_profile.hpp:39-40`。

本轮**最典型的「同一个定义式既当被检量又当期望量」**：

```cpp
// operation_profile.hpp:39-40   ← 默认值
double median_error_ratio{0.0};    // 留出验证误差
double p95_error_ratio{0.0};

// focused_benchmark.cpp:369-375   ← 只在有样本时才赋值
if (!cpu_errs.empty()) {
    op.cpu.median_error_ratio = cpu_errs[cpu_errs.size() / 2];
    op.cpu.p95_error_ratio  = cpu_errs[(size_t)(0.95*(cpu_errs.size()-1))];
}

// focused_benchmark.cpp:407-415   ← 与常量比
const bool cpu_err_ok = op.cpu.median_error_ratio <= 0.30 &&
                        op.cpu.p95_error_ratio   <= 0.60;
if (kind == Standard && cpu_err_ok && gpu_err_ok) {
    op.qualified = true;
    op.qualification_reason = "measured-qualified";
}
```

`cpu_errs` 为空 ⟺ 每折都被 `:365` 的 `if (actual > 0.0 && pred > 0.0)` 丢弃 ⟺ 计时全为 0。此时两个误差比**保持默认 0.0**，`0.0<=0.30` 与 `0.0<=0.60` **同时成立** ⇒ **零误差证据的 Operation 被盖上 `"measured-qualified"`**。

后果不是理论的：`op.qualified` 是路由顶层资格位（`benchmark_route_estimator.cpp:425`、`mixed_route_planner.cpp:40`）；`operation_profile.cpp:31-32` 把 0.0 原样序列化，**与"完美拟合"在产物里不可区分**；`validate_operation_profile`（`:249-311`）**从不检查任何误差比** ⇒ "schema 校验通过"拦不住。

子代理补出我漏掉的一半：**GPU 侧同洞更易触发**（`:378-380`）。`run()` 在 resident 样本为空时压入字面 `0`（`:206-210`）⇒ 一张"256K 能跑、更大尺寸失败"的卡产生 `[非零,0,0,0,0]`，`gpu_curve_ok` 只看 `[0]>0` 判 true，每折又在 `:394` 被丢弃 ⇒ **1/5 的 GPU 曲线被认证为误差可信**。

附带：`:372-374` 的 "p95" 取 `cpu_errs[(size_t)(0.95*(n-1))]`。n=5 → 下标 3 = **P80**；n=2 → 下标 0 = **两个里最好的那个**（子代理实测确认，且 n=2 可达）。

---

**F1 —— 「profile hash 运行前后不变」这条验收在结构上不可能失败**
`tests/classic/e18_workpool.cpp:250-276`

```cpp
253:    const char* path = "acr_e18_profile_hash.json";
255:        std::ofstream f(path);
256-258:  f << R"({...,"sha256":"e18hash"}})";   // ← 测试自己写的
260-262:  f1.tellg()  → size_before
264:        parallel_for(KernelId::Custom, Range1D{0,1000}, [](std::size_t) {});  // ← 空 lambda
266-267:  f2.tellg()  → size_after
270:    bool ok = (size_before == size_after);
```

三重失效：**判据文件由被检流程自己写**（ACR 运行时任何代码都不会打开它）；**被检操作是空 lambda**；**只比字节数不比内容**（把 `sha256` 改成同长度另一串照样 PASS）。文件头 `:8` 却宣称覆盖 `profile hash 运行前后不变` —— **代码里从头到尾没算过任何 hash**。

---

**F2 —— holdout 泛化测试先赋值再断言自己，且文档中的门限从未被断言**
`tests/unit/test_profile_holdout.cpp:187-196`、`:209-213`

```cpp
183:    // 门限：中位相对误差 <= 0.35、P95 <= 0.75。
187:    const bool axpy_unqualified = (median > 0.35 || p95 > 0.75);
193:        if (axpy_unqualified) cpu_curve->second.qualified = false;   // ← 测试写值
195:    EXPECT_EQ(cpu->arithmetic.at({...}).qualified, !axpy_unqualified); // ← 断言刚写的值
```

`unqualified` 分支断言的就是测试自己 `:193` 刚赋的值 ⇒ **恒真**。`:184-186` 注释甚至写明"holdout 未达标 → 标 unqualified" —— 作者知道它不达标，于是把不达标**改写成一个状态**再断言这个状态。**泛化失败永不转红。**

更重的是：**全文件不存在 `EXPECT_LE(median, 0.35)` 或 `EXPECT_LE(p95, 0.75)`**。门限只用来驱动 `:193` 的自赋值；holdout 误差 500% 的模型照样 PASS。`:210-213` 的 sum 版本同构，且整段断言被包在 `if (find != end())` 里 —— 曲线缺失时根本不执行。

---

**F3 —— 四个 ACR CLI 工具被 CMake 静默丢弃**
本片成员 `tools/acr_status/CMakeLists.txt:7` 与 `tools/acr_classic_runner/CMakeLists.txt:12` **永不生效**。

`lib/infrastructure/acr/CMakeLists.txt:231-232` 写的是 `eng/tools/acr_benchmark`、`eng/tools/acr_status`、`eng/tools/acr_report`、`eng/tools/acr_classic_runner`；真实目录在 `tools/acr_*`。我亲自 `ls` 核验：`eng`、四个 `eng/tools/*`、`buffers`、`backends/alpaka` **全部 MISSING**，`tools/acr_*` 四个 **全部 EXISTS**。`:235` 的守卫 `if(EXISTS .../CMakeLists.txt)` **无 else、无 message()** ⇒ configure 全绿地跳过。

后果：四个工具**一个都不会被构建**（尽管 `ACR_BUILD_TOOLS`=ON、`ACR_BUILD_CLASSIC_RUNNER`=ON）。本片 6 个 classic 测试全部通过 `extern "C" run_eXX()` 出口**只为该 runner 服务** ⇒ **E01–E21 经典实验在本仓没有任何可执行入口**。两个独立子代理各自发现，均判为"未被登记的新问题"。

---

## 3. 逐文件清单

### 生产源码（20 份）

| # | 文件 | 行 | 读到什么 → 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `include/astro/compute/acr.hpp` | 497 | `:339-364` `parallel_scan` 的 `KernelFn&& /*fn*/` **参数无名且完全不被调用**；`:348-352` 建了空 `scan_fn` 后 `(void)scan_fn;`；`:361` 立刻 `delete heap`。调用方传入的 kernel 被静默丢弃，返回纯前缀和 —— **静默降级出错误结果，不是失败**。`:124` `subview` 的 `offset+sub_count>count_` 可 size_t 溢出绕检查。`:59` `Extent2D::count()` 无溢出保护而 `:264` 依赖它判空。 | 须修 |
| 2 | `include/astro/compute/task_traits.hpp` | 202 | `:129-130` `kOpWeightedIntegrationFp64Acc`；`:145` `active_fraction_hint{1.0}`。该字段在生产中只被 `cost_estimator.cpp:64` 拼成字符串查找键，与写端键名**永久不匹配**（F9）。`:2`/`:100` 引用的两份规范文档未在本树命中（标为悬空待核）。 | 须修 |
| 3 | `scheduler/dispatcher.hpp` | 287 | `:163` 注释称 `per_device_stats` "仅由 completion 产生" —— **子代理逐链核实为真**（`dispatcher.cpp:1624-1633→1734-1743→2556-2566`），**我原先的质疑已撤回**。`:186-188` `fallback_count/error_count` 无生产写入者。`:62` `force_all_supported_executors` 靠注释约定"生产必须保持 false"，无断言。 | 建议 |
| 4 | `scheduler/fallback.cpp` / `.hpp` | 65 / 57 | `:20` `skip_already_done=true` 无条件；`:21` 注释承认 `pending_chunks` **包含失败设备正在执行的块** ⇒ 非幂等算子有重复执行风险。`:27-31` 策略为 `ToCpu`（**默认值**，hpp:54）时**无条件**返回 `target="cpu"`，**即使失败设备就是 cpu、即使 cpu 不在 available_backends 里** ⇒ 自我重派发。 | 须修 |
| 5 | `scheduler/reduction_merger.hpp` | 49 | 纯头（Pimpl），实现在 `.cpp`（**不在本片**）。`:8` "线程安全"、`:39` "多次调用安全" 为不可验证声明。 | 无判定 |
| 6 | `scheduler/residency_manager.cpp` | 224 | **F6/F7 全部在此**。`:105` `mark_uploaded(key)` **不接收 device**；`device_id` 只在 `:149/:160/:211` 被**读**，全文件**零写入**，默认 `"cuda:0"`。`:176-177` 未知键→`HostValid` 与 `:158/:168` 未知键→`true` **互相矛盾**（子代理证伪其可达性）。`:73-76` else 分支是**空实现**。`:90-95` 守卫**漏掉 `BothValid`**（子代理补出）。`:200-222` `status_json` 的 `"bytes"` 是**声明容量**不是传输量，hpp:8/hpp:108 的"报告实际传输字节"为**悬空声明**。 | 须修 |
| 7 | `utilization/config_hot_read.cpp` / `.hpp` | 164 / 99 | `:63-65` 未初始化时 `read()` 返回**静默默认**（0.95/0.95/2GiB/512MiB/BestEffort），无错误码。`:67-71` 四个热字段在**锁外**用四个独立 relaxed 原子读、`:74` 才加锁 ⇒ hpp:72 的"线程安全快照"**为假**。`:84/90/96/102/108` 先存原子后加锁、`:44` 先加锁后存原子 ⇒ **锁序反转**。`:83-116` 六个 `noexcept` 中 `:115` 的 `unordered_map::operator[]` 可抛 `bad_alloc` ⇒ `terminate`。`:14/:36` `cold_frozen` **全仓零读**。hpp:43 默认 `BestEffort`（fail-open），且与 `fallback.hpp:21` 的 `FallbackStrategy` 是**同名不同义、无转换层**的影子枚举。**子代理证伪严重性**：该模块零生产实例。 | 建议（严重性已下调） |
| 8 | `backends/cuda/cuda_bridge_loader.cpp` | 388 | **F17 全部在此**（`:224-243` 过期快照）。另：`:101` `if(!mod) return;` **完全静默**；`:152-154` 任一符号缺失即 `g_api = BridgeApi{}` **全量归零**（31 个符号的 AND），二者**都无错误码、无日志、无诊断出口**（同仓 NVML loader 在 `utilization/system_metrics.cpp:143-172` 有三种明确诊断，本文件一个都没有）。`:184-191` `queue_state()` 绕同步 launcher，`depth` 永不超过 1（子代理补：dispatcher 对每个非 CPU executor **硬编码 1 个 worker** `dispatcher.cpp:1142`，且所有读取都发生在该 worker 自己的 submit **之前** ⇒ depth 恒观测为 0 ⇒ `gpu_delay_ms` 恒 0 ⇒ BDR 恒判可缓存）。`:192-193`/`:376` 硬编码 `65536/256`，`set_recommended_chunk` **零调用点** ⇒ 该字面量是死的。`:14` 无条件 `#include <windows.h>`，`backends/cuda/CMakeLists.txt:10` 的注释写"始终编译"，**无平台守卫**（正确的守卫模式存在于 `coverage/CMakeLists.txt:57-58`）。`:181-183` `supports()` 落到 `kernel_registry.cpp:80` 的 `return reg->cuda.has_value();` —— **纯注册标志，零设备能力判断**。 | **阻断（F17）** |
| 9 | `backends/cuda/bridge/acr_cuda_bridge.h` | 242 | `:13` "所有函数返回 0 表示成功" —— 但 `:33-34` `acr_cuda_bridge_init` **返回设备数**，0 同时表示"无设备"与"驱动错误"，**违反本文件自己的错误约定**；加载器 `:372-373` 拿到 ≤0 直接 `return`、**把 `err` 丢掉**（`dispatcher.cpp:159-169` 同样丢）⇒ 两者永久不可区分。`:233-236` 把"frames 上传必须保持 1"写成验收条款，**无任何测试守护**。`:56` "全部为同步语义"对 `:143/:152` 纯上传与 `:160-185` 的 `_resident` 不成立。 | 须修 |
| 10 | `backends/cuda/cuda_executor.hpp` | 79 | 整文件被 `#ifdef ACR_BUILD_CUDA` 包住 ⇒ **默认构建下为空**。`:33-34` 注释里的 `65536/256` 与实现同为硬编码。`:11` "submit 是同步的"。 | 建议 |
| 11 | `backends/cpu/isa/avx.cpp` | 43 | `:36-41` 的 `caps.has_isa(AVX)` 是**真实运行时门**（非恒真门），`:19` 有标量尾兜底。**本片少数干净文件之一。** | 通过 |
| 12 | `core/task_descriptor.cpp` | 40 | `:35-38` 用 `< 0.0 || > 1.0` ⇒ **NaN 两比较皆假 → 判为合法**；`:12` 只校验一个字段，函数名却叫 `_valid`；**全片无 NaN/±inf 单测**。 | 须修 |
| 13 | `qualification/focused/focused_benchmark.cpp` | 679 | **F0 全部在此**。另：`:474-475` 三元两分支同为 `"fp32"` 的死条件；`:486-497` `step=size/nsz` **无 `step*nsz==size` 断言**；`:500/:519` **uint64 先减后转**（子代理算出后果与我原判**相反**，见 §6）；`:244` `run_gpu_operation` **返回值被丢弃**，而同文件 `:179` 的 GPU host 路径**检查了**返回值；`:262` 函数内 `static void* gh` 跨次调用复用句柄不重验；`:87` 空 `catch(...)`；`:664` GPU 未实测时写 `ns_per_item=cpu_slope*10+1` **伪造 GPU 读数**进产物；`:129` `(void)kind` ⇒ quick 与 standard 跑**完全相同**的扫描；`:579-580` `/4` 魔数、`:563-577` ±5% 阈值均无出处。 | **阻断（F0）** |
| 14 | `routing/benchmark_route_estimator.cpp` | 610 | ①`:378-396` 模拟器尾块 `take=min(chunk,remaining)` 常**小于实测最小块** ⇒ `:150-153` 拒绝 ⇒ `:397 continue` **丢弃整个候选** ⇒ `:406` 返回 −1 ⇒ `:579` 谎报为 `"mixed-not-validated"`（子代理证明死区 `frame_count ∉ [4,32]`、`total_items < 65536`）。②`:469-472` guard=0 ⇒ **零误差惩罚**，且两个校验器**从不检查误差比**。③`:509-511`/`:543-545` `vram_available_bytes==0` 判**通过** ⇒ 显存真为 0 时放行。④`:567` `max(0.0, simq-sim0)` 静默吞掉 `simq<sim0`（子代理给出数值构造）。⑤`:237` `w0*3/2` 是**整数除法**（w0=5 时实为 1.4x，注释写 1.5x）。⑥`:39-48` 两端**先写 `ms/p90` 再返回 false**。⑦`:302-304`/`:318-321` **忽略返回值**（靠 `:288-295` 的 usable 过滤兜住）。 | 须修 |
| 15 | `diagnostics/hardware_report.cpp` | 82 | `:48-49` CAS 首次生效但**无返回值**，调用方无法知道注册被忽略。`:62` `gpu_cb()` **未 try/catch**，回调抛异常 ⇒ 整个报告失败。`:75-76` `__DATE__/__TIME__` 写进指纹 JSON —— 若参与画像有效性比较则**每次重建指纹必变**。 | 建议 |
| 16 | `examples/legacy_chunk_adapter.cpp` | 66 | 示例代码，`:62` 打印 `no` 并返回 1 —— **失败可见**，不冒充通过。 | 通过 |
| 17 | `examples/weighted_integration/weighted_integration_common.hpp` | 127 | ①`:44-46` 在**每像素** worker 内 `throw std::runtime_error`，异常跨并行边界传播语义未定义，且**不是稳定错误码**。②`denominator = Σw`（`:42`）**不含 p** ⇒ 与 p 无关却被每像素重算 F 次、并在 p=0 就抛。③`:123` ref 全零时 `relative_l2 = +inf`，而 `:116` 的 `finite` **只检查元素不检查统计量** ⇒ **`finite=true` 与 `relative_l2=inf` 同时报出**。④`:117-118` `std::max(max_abs,NaN)` **静默吞掉** NaN 差值。 | 须修 |
| 18 | `qualification/benchmarks/convolution_benchmark.cpp` | 142 | ①`:112` `ops=w*h*k*k*2` 对**零填充边界像素同样计数**，而 `:36/:39` 的 `continue` 实际跳过那些乘加 ⇒ **MOp/s 系统性高估**（w=h=64,k=7 时约 18%），且进 CPU 画像的 Convolution 族。②`:64-68` 从 **benchmark 名字字符串**解析核大小，未命中时静默默认 3 ⇒ **命名写错会产出贴错标签的基准**。 | 须修 |
| 19 | `qualification/benchmarks/benchmark_main.cpp` | 102 | ①`:99` **`RunSpecifiedBenchmarks()` 返回值被丢弃** —— 内部 `SkipWithError` 后仍 exit 0 ⇒ CI 看到成功。②`:54` `--output` 在末尾时 `i+1<argc` 为假 ⇒ 原样透传给 Google Benchmark，报"无法识别参数"而非"缺少参数"。 | 须修 |
| 20 | `tools/acr_benchmark/main.cpp` | 111 | ①`:86` **硬编码** `raw_benchmark_records.json` 写到当前工作目录、**无视 `--output`** ⇒ 写未声明文件。②`:82` `driver.run()` 返回空时**不报错**，仍写出"权威"的 `hardware-profile.json` 并 exit 0 ⇒ **空画像被当生产上报**。 | 须修 |

### 构建 / 文档（8 份）

| # | 文件 | 行 | 读到 → 判定 |
|---|---|---|---|
| 21 | `qualification/CMakeLists.txt` | 119 | `:56-66` 列 11 个 benchmark 源，子代理逐一核验**全部存在**；`:47` 的 Declare 在 `CMakeLists.txt:127` **存在**（我方初判"悬空"被子代理**证伪**）。`:43` 强制关 Google Benchmark 的 `-Werror`。**通过** |
| 22 | `cost/CMakeLists.txt` | 18 | 正常。**通过** |
| 23 | `tests/CMakeLists.txt` | 11 | `:3-11` 五个子目录，子代理核验**全部存在且各有 CMakeLists**。`:9-10` 坦承 sanitizer 目标"此前只存在于源文件头注释的手敲 cl 命令行里"。**通过** |
| 24 | `tests/fault/CMakeLists.txt` | 76 | ①`:12` 定义 `ACR_BUILD_SANITIZER`，根 `CMakeLists.txt:54` 定义的是 **`ACR_ENABLE_SANITIZER`** ⇒ **两个不同名字的同一开关**。②`:26` `RUN_SERIAL TRUE # 并行负载下偶发 SEGFAULT（预存共享状态）` ⇒ **把已知竞态写进构建文件并用串行化绕开**。③`:49-56` 默认 OFF ⇒ `acr_test_sanitizer_actual` 编译并**恒 PASS 而不验证任何东西**。**须修** |
| 25 | `tools/acr_status/CMakeLists.txt` | 15 | 文件本身无缺陷，但**永不被 CMake 读取**。**阻断（因 F3）** |
| 26 | `tools/acr_classic_runner/CMakeLists.txt` | 22 | 同上。**阻断（因 F3）** |
| 27 | `docs/dependency-lock.json` | 196 | ①**12 个 `commit` 无一为 SHA**：10 个填 tag/版本（`:10,25,41,56,71,86,102,117,132,147`），2 个填哨兵 `:162 "system"`、`:177 "none"` ⇒ **该"锁"不锁任何东西**。②`:24-25` oneTBB tag 写双值而 commit 记 planned，`:35` 说实际用 MSYS2 2023.0.0 ⇒ **自相矛盾**，且 `CMakeLists.txt:123` 正把 planned 值接进构建。③`:85-96` GoogleTest 同矛盾，而 `ADR-006:1,8,30` 通篇断言 1.15.2 且**无 superseded 标记**。④`alpaka:15`/`CLI11:107`/`spdlog:137`/`fmt:152` 均标 `optional:false` 却全仓无真使用（`acr_benchmark/main.cpp:8` 明写"避免引入 CLI11"）；子代理补：三者**只 Declare 从不 MakeAvailable**，`alpaka_*` 选项设置全是惰性的。⑤`:193` `cache_dir` 对 CMake **零作用**（无任何 CMake 读此 JSON）。**须修** |
| 28 | `docs/ADR-006-googletest.md` | 77 | ①`:1,8,30` 断言 GoogleTest v1.15.2，与 `dependency-lock.json:85,96` 的 actual 1.17.0 **直接矛盾**；两份同处模块细节层、AGENTS.md 未给仲裁，而 ADR-006:7 自己把锁指定为版本记录 ⇒ **ADR-006 是过期件**。②`:71` "行覆盖 ≥80%" 是**不可执行的判据**，全仓无覆盖率报告。③`:69-75` 验收表**只有意图，无一条实测输出/产物路径**（违反 AGENTS §4）。④`:77` 承诺的 `run/logs/acr/gtest/<YYYYMMDD>/` **无任何代码创建**（`run/logs` 磁盘上不存在，全仓 11 处命中全是文档）。**须修** |

### 测试（15 份）

| # | 文件 | 行 | 关键发现 | 判定 |
|---|---|---|---|---|
| 29 | `tests/unit/test_scheduler.cpp` | 1462 | `:1382-1383` `EXPECT_GE(size_t,0u)` ⇒ **无符号数比 0 的恒真门**，而测试名正是 `RecoverableGateStatsInitialized`。`:1152-1156` 先 `std::sort` 再断言 `front()<=back()` ⇒ **排序后必真**，且是 `DynamicModeTailConvergence` 里唯一触及"尾部收缩"的断言。`:1341` 注释写"不小于 min_chunk"而断言写 `<=` ⇒ **注释说 A、代码验 B**，clamp 失效（返回 50）与生效（返回 100）**都能通过**。`:202` `EXPECT_GE(f,1e6)` 对真值 ~1.058e6 过松。`:1160-1187` 用 `+=1` 累加，是全片**唯一真正能检测重复执行**的无重叠测试。 | 须修 |
| 30 | `tests/unit/test_dispatcher_bdr.cpp` | 826 | `:302,354,411,589,654` 五处 `GTEST_SKIP()` ⇒ **驻留/generation 一致性契约在任何无 CUDA 的机器上零覆盖且与通过不可区分**。`:63-70,103-115,124-125` 合成 profile 把 `qualified/routing_trusted/model_available/p95_error_ratio` **全部置真** ⇒ 资格门只在全真配置下被测。`:189` `bytes_read_per_item = frames*4u+4u` **硬编码 4**，无视传入的 `element_size` ⇒ `BufferBytesFollowElementSize` 声称验"真实字节驱动记账"，实际只验了 buffer 侧。`:681,692` 用 `EXPECT_EQ(...,1u)` 而 `:693` 用 `EXPECT_GE(...,1u)`，同一契约两槽严格度不一致。 | 须修 |
| 31 | `tests/unit/test_resource_control.cpp` | 353 | **`:149-152` 的注释本身就是缺陷自白**："set_cache_release_hook 全仓生产调用点为 0 ⇒ cache_release_hook 恒空 ⇒ ReleaseCache 分支里释放缓存这一动作根本没有执行"。`:105-144` 真正跑 hook 的用例**在 `:117` 自己造 hook**，测的是产品永不可达的配置。`:168-171`/`:213-216` 的否定循环**未断言 `control_actions` 非空** ⇒ 整体删除记录机制仍 PASS。全部 7 个用例经 `memory_sampler_override`（`dispatcher.hpp:96` 注明"生产路径为 null"）驱动 ⇒ **真实采样器零覆盖**。 | 须修 |
| 32 | `tests/unit/test_cpu_profile.cpp` | 335 | `:311-315` 断言 5 个 overhead 键存在，而 `profile_generator.cpp:495-501` **无条件插入** ⇒ 恒真。`:317-320` 注释点名 `1100/8500` 却断言手挑的 `<5000`。`:188,208` 只断言 `points.size()>=1`。 | 建议 |
| 33 | `tests/unit/test_profile_holdout.cpp` | 214 | **F2 全部在此**。另 `:121` `if(actual>0 && predicted>0)` 把"预测失败"的点**静默剔除**而非计为灾难误差。`:159` 整块 GPU 排序门在无 GPU 时**静默消失**（无 `GTEST_SKIP`）。 | **阻断（F2）** |
| 34 | `tests/unit/test_task_descriptor.cpp` | 187 | 覆盖扎实（`:114-139` 的 work_size 优先级三层隔离正确，`:37,63` 的反向 range / 零 extent 是真边界）。**唯一缺陷：无 NaN/±inf 用例**，正是 `task_descriptor.cpp:36` 那个洞的盲区。 | 通过（附缺口） |
| 35 | `tests/unit/test_qualification.cpp` | 169 | `:61` `EXPECT_EQ(BENCHMARK_FIXED_SEED, 0xA57C5AC20260802ULL)` 与 `profile_schema.hpp:32` 的 `constexpr` **逐字节相同** ⇒ 断言编译期常量等于它自己，**不能失败**；而 `:4` 声称覆盖"确定性"—— 实际**从未跑两遍比较**。`:136-140` 的 FIPS 向量是全片**最强的一条断言**。 | 须修 |
| 36 | `tests/classic/e18_workpool.cpp` | 321 | **F1 全部在此**。另 `:211-212` `coverage.done==coverage.total` 两字段来自**同一被检对象自报**，只能证明自洽；`all_assigned` 用 `=` ⇒ **无法检测重复执行**（对比 test_scheduler:1178）。`:88-90,135-139,181-185` SKIPPED 时 `make_result` 第 5 参传 **`true`**，仅靠 gtest 层 `GTEST_SKIP` 补救。 | **阻断（F1）** |
| 37 | `tests/classic/e09_gather_scatter.cpp` | 302 | 判据扎实：`:81,102,142,168,217-223` 参考值**独立串行计算**，`:90,152` **逐位相等**。缺陷：`:129-141` 拒绝采样只把 `active` 截到 `n`、**未截到 `dst_n`** ⇒ `active>dst_n` 时**死循环**（当前用例未触发）。`:237` 容差约为真实 fp32/fp64 差的 800 倍。`:43,46` 未校验 `range_n>0`。 | 建议 |
| 38 | `tests/classic/e20_fault_fallback.cpp` | 277 | **本片最诚实的故障注入文件**：`:174-201` 做**正负两例**（空 coverage 时 `skip_already_done` 必须为 false），`:42,106` `catch(...)` 至少要求"必须抛"。缺陷：`:42,106` 丢异常身份（真 OOM 与任意异常不可区分）；`:156-159,74-77` 三次计时调用的 Event **全部丢弃**。 | 须修 |
| 39 | `tests/classic/e21_persistence_concurrency.cpp` | 273 | `:149` `bool ok = true; // 不崩溃即 ok` —— 造 50 个 Event 做 construct/wait/cancel/move 后**断言字面量 true**；`:213` 同样。`:186-199` sanitizer 检测：`:186` 先置 true、`:194` `#else` 又置 true；`:192` 注释说"标记 SKIPPED"、`:198` 发的却是 **PASS**。`:2` 头注释要求"30 秒及更长"，`:28` 默认 **5 秒**、`:222` 用 `max(2, 5/2)` = **2 秒**。 | 须修 |
| 40 | `tests/classic/e03_dot.cpp` | 178 | `:149-150` `fp32_close(a,r) \|\| (max_abs<=rel_tol)` 的**第一个析取项是死臂**（子代理证伪：第二项在 `ref>=0.2` 时恒更宽，而 ref ≈ 0.577√n ≫ 0.2）。`:144-146` 注释宣称"相对误差必须 <1e-4"，代码**不强制**。`:45,70,93,117` 当 `|ref|<=1e-30` 时把 `max_rel` 置 **0.0**（最优值）—— 与 `weighted_integration_common.hpp:123` 的 `+inf` 是同一退化情形的**两种相反错法**。`:46,94` `rmse = max_abs`（**字段名不符**）。`:48,95` 容差约为 n=1M 真实误差的 1.5 万倍且无出处。 | 须修 |
| 41 | `tests/classic/e02_axpy.cpp` | 133 | `:34/40,56/62,76/81,96/102` **被检表达式与参考表达式逐字符相同、常量逐字相同** ⇒ 参考用同一编译器、同一 FP-contraction 自由度 ⇒ **FMA 构建与 mul-then-add 构建都能通过**；`:3` 声称"验证 FMA 行为"，实际**一次 `fma()` 都没调用**。`:44,85,106` 的 `*2.0`/`*1.0` 是数据幅度代理，**无推导**（`kA=1.75` 时真上界 2.75）。`:38,60,100` **把 `std::copy` 放进计时 lambda** ⇒ 报的 AXPY 耗时含整数组 memcpy，这些数字进 CPU Arithmetic 画像。 | 须修 |
| 42 | `backends/cuda/bridge/acr_cuda_bridge.h` | 242 | 见第 9 项。**须修** |
| 43 | `classic_common.hpp`（片外，仅取证） | — | 所有 classic TEST 体都是 `ResultSink::push(r); EXPECT_TRUE(r.correct);` —— gtest 层断言粒度为零。 | 建议 |

---

## 4. 发现清单

### 4.1 阻断（Blocking）—— 5 条

| ID | 一句话 | 主证据 |
|---|---|---|
| **F17** | **`prefetch_inputs` 在同一次调用里把自己刚上传的输入挤掉** ⇒ 加权积分的 `d_x` 里装的是权重数组 ⇒ **静默产出错误测光结果，却报成 GPU 执行成功、`all_done=true`** | `cuda_bridge_loader.cpp:224-243`（+`:256-261`）→ `dispatcher.cpp:2477-2485,2492` → `weighted_integration_kernels.cpp:125-129` → `acr_cuda_bridge_host.cpp:1006` |
| **F0** | **资格门恒开**：零误差样本 → 默认 0.0 → `0.0<=0.30` 恒真 → `qualified`；GPU 侧 `[非零,0,0,0,0]` 更易触发 | `focused_benchmark.cpp:369-375,378-380,394,407-415` + `operation_profile.hpp:39-40` |
| **F1** | **「profile hash 不变」判据结构上不可能失败**：判据文件由测试自己写、被检操作是空 lambda、只比字节数 | `e18_workpool.cpp:250-276`（对比头注释 `:8`） |
| **F2** | **holdout 先赋值再断言自己**，且文档门限 `0.35/0.75` **全文件从未被断言** ⇒ 泛化失败永不转红 | `test_profile_holdout.cpp:187-196,209-213` |
| **F3** | **四个 ACR CLI 工具被 CMake 静默丢弃** ⇒ 本片 2 个工具 CMakeLists 是死文件，E01–E21 链**无任何可执行入口** | `lib/infrastructure/acr/CMakeLists.txt:231-232,235` + `tools/acr_status/CMakeLists.txt:7` + `tools/acr_classic_runner/CMakeLists.txt:12` |

### 4.2 须修（Must fix）—— 15 条

| ID | 一句话 | 主证据 |
|---|---|---|
| **F6** | `ResidencyManager` 的 `device_id` **全文件零写入**（默认 `"cuda:0"`），dispatcher 5 处查询**全部硬编码 `"cuda:0"`** ⇒ 多卡下把"卡0 有副本"当成"所有卡有副本"，而 `dispatcher.cpp:2194` 拿它驱动 BDR 路由 | `residency_manager.cpp:105,149,160,211` + `residency_manager.hpp:43` + `dispatcher.cpp:1857,1880,2165,2194,2227` |
| **F7** | generation **双向计数器混用**（`mark_host_dirty` 本地 `++` vs `register_or_update` 外部覆盖）⇒ 可回退；回退后 `!=` 判假 ⇒ `invalidate_input` 不被调用 ⇒ **陈旧 device view 被当作有效** | `residency_manager.cpp:68,77,96` |
| **F18** | `ensure_bridge_loaded()` **完全静默**（`:101` DLL 缺失无任何输出；`:152-154` 31 个符号缺一个即全量归零）；同仓 NVML loader 有三种明确诊断可作对照 | `cuda_bridge_loader.cpp:101,152-154` vs `utilization/system_metrics.cpp:143-172` |
| **F19** | `queue_state()` 观测到的 `depth` **恒为 0**（executor 硬编码 1 worker，所有读取都发生在该 worker submit 之前）⇒ `gpu_delay_ms` 恒 0 ⇒ BDR 恒判可缓存，**直接废掉 `:2311-2313` 声明的"queue busy 时旁路缓存"** | `cuda_bridge_loader.cpp:184-191` + `dispatcher.cpp:1142,2297-2321` |
| **F8** | `task_traits_valid` **NaN 通过**（`<`/`>` 对无序比较皆假），且全片无 NaN 单测 | `task_descriptor.cpp:35-38`；`test_task_descriptor.cpp:173-187` |
| **F9** | **稀疏算子成本模型恒失效**：读端 `"gather:random:"+hint`，写端 `"gather:random"` ⇒ 精确匹配**永不命中** ⇒ `active_fraction_hint` 对成本**零影响** | `cost_estimator.cpp:64` vs `profile_generator.cpp:452` vs `hardware_profile.hpp:98`（我已亲自读三处原文） |
| **F10** | `simulate_mixed` **尾块超出实测块区间 ⇒ 整个候选被丢弃 ⇒ Mixed 不可行**，理由被谎报为 `"mixed-not-validated"`；死区 `frame_count ∉ [4,32]`、`total_items < 65536` 可证明 | `benchmark_route_estimator.cpp:150-153,378-397,406,564,579` |
| **F11** | `guard_of` 两误差比皆 0 时 **零误差惩罚**，且两个 profile 校验器**从不检查任何误差比** ⇒ 该 fail-open 通过 schema 校验 | `benchmark_route_estimator.cpp:469-472,484-488,570` |
| **F12** | `vram_available_bytes == 0` 判为**通过** ⇒ 显存真为 0 时门放行（伪装成"未知"的 fail-open） | `benchmark_route_estimator.cpp:509-511,543-545` |
| **F13** | `parallel_scan` **完全丢弃调用方传入的 kernel**（参数无名、scan_fn 建后即弃、heap 立刻 delete），静默返回错误结果 | `acr.hpp:339-364` |
| **F14** | `weighted_integration_common.hpp`：`finite=true` 与 `relative_l2=+inf` 可**同时报出**；`std::max(max_abs,NaN)` 静默吞 NaN；且在**并行 worker 内逐像素抛非稳定错误码异常** | `weighted_integration_common.hpp:44-46,116-124` |
| **F15** | `dependency-lock.json` 的 `commit` **12 个无一为 SHA**（10 个填 tag、2 个填 `"system"`/`"none"`）⇒ 该"锁"不锁任何东西；并与 ADR-006 在 GoogleTest 版本上直接矛盾 | `dependency-lock.json:10,25,41,56,71,86,102,117,132,147,162,177` + `ADR-006:1,8,30` |
| **F20** | `acr_cuda_bridge_init` 返回设备数 ⇒ **0 同时表示"无设备"与"驱动错误"**，违反同文件 `:13` 的错误约定；加载器与 `dispatcher.cpp:159-169` **都丢弃 `err`** | `acr_cuda_bridge.h:13,33-34` + `cuda_bridge_loader.cpp:371-373` |
| **F21** | 上传失败（`upload_persistent_slot != 0`）在 `:250-252` **提前 break**，跳过 `:256-261` 的视图更新 ⇒ 管理器仍声称旧 host 驻留，而桥接侧 `ensure_buffer` **已 `cudaFree` 旧缓冲且不拷贝** ⇒ 声称驻留的数据已被销毁 | `cuda_bridge_loader.cpp:250-261` + `acr_cuda_bridge_host.cpp:138-146` |
| **F22** | CUDA 版 `prefetch_inputs` **丢弃了基类的 `hosts.size()==bytes.size()` 前置检查**，`bytes[i]` 无保护下标访问 | `cuda_bridge_loader.cpp:217-264` vs `device_executor.hpp:88` |

### 4.3 建议（Suggestion）—— 14 条

| ID | 一句话 | 主证据 |
|---|---|---|
| S1 | 每个 classic TEST 都是 `push(r); EXPECT_TRUE(r.correct)`，gtest 层断言粒度为零 | 全部 classic 文件 |
| S2 | e03/e09 容差约为真实误差的 800～15000 倍且无出处 | `e03:48,72,95`；`e09:237` |
| S3 | E02 参考式与被检式逐字符相同 ⇒ 声称的 FMA 行为**不可测** | `e02:34/40,76/81` |
| S4 | E02 把 `std::copy` 放进计时 lambda ⇒ 画像里的 AXPY 耗时含整数组拷贝 | `e02:38,60,100` |
| S5 | e21 头注释承诺 30s，实际默认 5s / 2s，环境变量可静默降级 | `e21:2,28,222` |
| S6 | e21 sanitizer 检测恒 PASS 且注释说 SKIPPED、代码发 PASS | `e21:186-199` |
| S7 | `set_dynamic_max_chunk` 用例**测不出 clamp 是否生效**（注释说 A、断言验 B） | `test_scheduler.cpp:1338-1341` |
| S8 | 8 处 `GTEST_SKIP` 使 GPU/驻留/generation 契约在无 GPU 机器上零覆盖且与通过不可区分 | `test_dispatcher_bdr.cpp:302,354,411,589,654`；`e18:283,290,297` |
| S9 | 未知 OperationId 兜底 ResidentChain 在**两处**出现（`qualify` 侧 + `op_from_id` 内核派发侧）；后者喂**共享的 CUDA launcher** | `focused_benchmark.cpp:448`；`focused_operations.cpp:49-54,599` |
| S10 | `test_resource_control` 全部用例走**测试专用注入缝隙**，真实采样器零覆盖 | `test_resource_control.cpp` 全部 + `dispatcher.hpp:96` |
| S11 | `config_hot_read` 三处内部不自洽 + 六个 `noexcept` 含分配；**但整个模块零生产实例**，修它会产生"绿色假象" | `config_hot_read.cpp:63-65,67-74,83-116` |
| S12 | `ACR_BUILD_SANITIZER`（fault:12）与 `ACR_ENABLE_SANITIZER`（根:54）**同名不同名两开关**；`RUN_SERIAL # 偶发 SEGFAULT` 把已知竞态写进构建文件 | `tests/fault/CMakeLists.txt:12,26` |
| S13 | `#include <windows.h>` 无守卫、`acr_cuda_bridge_loader` **始终编译**；产品树靠"不被 add_subdirectory"而非平台守卫幸免 | `cuda_bridge_loader.cpp:14` + `backends/cuda/CMakeLists.txt:10` |
| S14 | 多 stream 能力全死代码（`set_streams`/`max_in_flight` 零生产调用者）；`recommended_chunk()` 是死字面量（`set_recommended_chunk` 零调用点）；`65536,256` 在 `:376` 与 `dispatcher.cpp:167` **硬编码两份** | `cuda_bridge_loader.cpp:192,281-292,376` + `dispatcher.cpp:167` |

---

## 5. 我主动构造的反例

> 构造什么 → 期望推翻什么 → 是否推翻。全部只用读码推导，**未运行任何二进制**。

### R1 —— 对 F0（资格门恒开）
**构造**：让 `run_cpu_operation` 在 5 个尺寸上全返回 0。⇒ `cpu_ns=[0,0,0,0,0]` ⇒ 每折被 `:365` 丢弃 ⇒ `cpu_errs` 空 ⇒ 误差比保持默认 0.0 ⇒ `cpu_err_ok` 真 ⇒ `qualified=true`，理由 `"measured-qualified"`。
**期望推翻**：「空样本不会通过门」。**结果：未推翻，反例成立**（我亲自读 `operation_profile.hpp:39-40` 确认默认确为 0.0）。子代理补出 GPU 侧高概率路径与我 n=2 时 p95 取到最好样本的退化。

### R2 —— 对 F1（profile hash 判据）
**构造**：让 ACR 运行时把 `sha256` 从 `"e18hash"` 改成 `"deadbeef"`（同为 8 字符）。
**期望推翻**：`:270` 转红。**结果：未推翻，反例成立** —— 长度不变，`ok` 仍 true。更强：**把整个 ACR 运行时删掉，该测试依然 PASS**。

### R3 —— 对 F2（holdout 泛化门）
**构造**：让 AXPY holdout 中位误差 = 5.0（远超文档门限 0.35）。
**期望推翻**：应触发 `EXPECT_LE(median,0.35)` 转红。**结果：未推翻，反例成立** —— 全文件不存在该断言。子代理独立复核并补出 `:210-213` 的断言还被包在 `if` 里。

### R4 —— 对 F17（新阻断，两输入挤掉）
**构造**：两槽先被 `{F1,W1}` 占住；再以新帧栈 `{F2,W2}` 调 `prefetch_inputs`。
**期望推翻**：`in_set` 快照过期不应导致第二次分配重选 slot0。**结果：未推翻，反例成立** —— `:224-231` 在循环外算 `in_set`，`:241-243` 在循环内重读这份过期快照 ⇒ i=1 再次选中 slot0 ⇒ **F2 被 W2 覆盖**而函数返回 `true`，调用方把两者都标记为已上传。子代理给出了完整到 `SubmitStatus::Ok / all_done=true` 的链路。

### R5 —— 对 F6（device_id 永不记录）
**构造**：`mark_uploaded("frames")` 在**真实上传到 cuda:1** 之后调用。
**期望推翻**：`is_device_valid("frames","cuda:1")` 应为 true。**结果：未推翻，反例成立** —— `mark_uploaded` 签名根本不接收 device；`device_id` 只被读、零写入。我进一步核实 `dispatcher.cpp` 5 处查询全部硬编码 `"cuda:0"`。
**诚实修正**：我最初断言"cuda:1 永久重复上传"—— 子代理**证伪**（既然所有查询都是 `"cuda:0"`，就不会重复传）。真实危害是**反方向的假阳性**：`is_device_valid(key,"cuda:0")` 为真 ⇒ `dispatcher.cpp:1880` 跳过上传、`:2194` 把 `was_resident` 置真 ⇒ 路由按"设备驻留"决策，而 2 号卡从未收到 H2D。**已改写。**

### R6 —— 对 F10（Mixed 尾块死区）
**构造**：`total_items=131172`，候选 `{65536,262144,1048576}`（CPU）与 `{262144,1048576,4194304,16777216}`（GPU）。
**期望推翻**：至少存在一个 `(c,g)` 能跑完。**结果：部分未推翻** —— `sim0` 中两设备 ready 均 0 ⇒ CPU 恒先领，该组合存活。子代理给出更强、**可证明**的死区：`frame_count ∉ [4,32]`（实测帧数只有 {4,16,32}）与 `total_items < 65536`。
**同时证伪了我的一条子断言**：「首个 take 可能超过 back」**不可能**（`take=min(chunk,remaining)`，`chunk` 恒为实测值）。**已删除。**

### R7 —— 对 F9（曲线键永久不匹配）
**构造**：把 `active_fraction_hint` 设为 0.05，问 `cost_estimator` 能否命中 `profile_generator` 产出的曲线。
**期望推翻**：键名一致即可命中。**结果：未推翻，反例成立** —— 我亲自读三处原文：读端 `cost_estimator.cpp:64` `"gather:random:"<<hint`（**恒有后缀**）、写端 `profile_generator.cpp:452` `"gather:random"`（**无后缀**）、文档 `hardware_profile.hpp:98` `"gather:random:0.05"`（**有后缀，站读端**）⇒ **任何 hint 都 miss**。

### R8 —— 对 F13（parallel_scan 丢 kernel）
**构造**：调用 `parallel_scan(..., myScanFn, myOp)`，其中 `myScanFn` 把 `in[i]` 翻倍。
**期望推翻**：结果应体现翻倍。**结果：未推翻，反例成立** —— `acr.hpp:341` 参数写作 `KernelFn&& /*fn*/`（**无名**），`:348-352` 的 `scan_fn` 体为空、`:360-361` `(void)scan_fn; delete heap;`。**用户的 kernel 从未被调用。**

### R9 —— 对 F3（工具不入构建）
**构造**：数一数 ACR 树里有多少条 `add_subdirectory` 能到达 `tools/acr_status`。
**期望推翻**：至少有一条。**结果：未推翻，反例成立** —— 我亲自 `ls` 确认 `eng` 与四个 `eng/tools/*` **全部不存在**，`:235` 的 `if(EXISTS)` 无 else、无 message。

---

## 6. 盲复算（遮住既有判定独立取证）

**方法**：先写下判定，再对 8 个关键点**遮住结论、只从源码重新推导**，然后比对。

| # | 既有判定 | 盲复算独立取证 | 一致性 |
|---|---|---|---|
| 1 | F0 资格门恒开成立 | 遮住结论重走：`DeviceCurve` 有无其他初始化器？→ `operation_profile.hpp:56-65` 无 ⇒ 默认 0.0；赋值是否在 `if` 内？是；门是否与常量比？是 | **一致（确认）** |
| 2 | 我曾判 `test_scheduler.cpp:190` 为自洽式断言 | 从 `queue_aware.cpp:12-28` 的公式独立重推 CPU finish = 0+0+1000×10+1000 = 11000，**与常量相同但推导路径独立** | **偏严 → 已撤回** |
| 3 | 我曾判 residency 未知键 `HostValid` 造成 fail-open | 全仓清点 `state()/needs_upload()/needs_download()` 调用者 | **偏严 → 已下调**（三者**零生产调用者**，是潜伏陷阱） |
| 4 | 我曾判 `config_hot_read::read()` 是活跃 fail-open | 清点 `ConfigHotReader` 的实例化点 | **偏严 → 已下调**（**零生产实例**；子代理补：该 fail-open 被 `test_utilization.cpp:276-282` 反而**断言成了绿**） |
| 5 | 我曾判 `active_fraction_hint` 的 NaN 会算术传播 | 只找数值乘法 | **偏严 → 已撤回**（唯一使用点是拼字符串） |
| 6 | 我曾判 `simulate_mixed` 因首块超界而全死 | 逐步走 `take=min(chunk,remaining)` | **部分偏严 → 已修正**（`take ≤ back` 恒成立；改用可证明死区） |
| 7 | 我曾判 `op_id_to_enum` 未知 id 导致 weighted_integration 被误评分 | 查 `qualify()` 的所有调用点 | **偏严 → 已撤回**（只接受 `build_profile()` 的输出，5 个 id 构成双射；改列为「潜在不可达」） |
| 8 | **我曾判 CUDA `submit()` 无条件上报 `items_done/bytes_done` 是"伪造核算"，且 `elapsed_ns` 恒为 0** | 遮住结论，追 `inv.domain` 与 `token` 的关系、再全仓找 `set_tls_elapsed` 调用点 | **偏严 → 已全部撤回**：dispatcher 设 `inv.domain = WorkDomain{token.begin, token.end}`（`dispatcher.cpp:1614,1682`；GPU-direct 方向相反 `:886-890`），**执行区间恒等于 `token.size()`**；`set_tls_elapsed` 被**每一个生产 launcher** 调用（`classic_kernels.cpp:111,129,147,176`；`focused_operations.cpp:499,589`；`weighted_integration_kernels.cpp:140`）。**这两条我写进初稿后被证伪，已从交付件删除。** |

**结论**：本轮我对自己的 **8 条判定中有 7 条存在不同程度「偏严」**，全部被子代理证伪或降级，已在正文逐条改写或删除。**保留为阻断的 5 条（F17/F0/F1/F2/F3）均经至少两路独立取证。**

---

## 7. 子代理派发记录

派发 **7 个**（要求 3–5；因一次误重复派发，实际 7 个），**7 个全部回执**：

| # | id | 范围 | 结论 |
|---|---|---|---|
| 1 | `8b4aa500` | residency_manager / config_hot_read / task_traits | 已回 |
| 2 | `0713ae43` | 同上（第二路交叉验证） | 已回 |
| 3 | `aa77a1cf` | 构建引用 / dependency-lock / CMake 选项 | 已回 |
| 4 | `8dbe0d25` | 同上（第二路） | 已回 |
| 5 | `54b4cbdb` | 13 个测试文件盲审（断言可判别性） | 已回 |
| 6 | `e76cac7f` | focused_benchmark / benchmark_route_estimator | 已回 |
| 7 | `128e1710` | cuda_bridge_loader / launcher 契约 | 已回（**推翻我两条，补出 F17**） |

### 逐条复核与否决记录

**我否决/降级/删除了 12 条（含我自己 6 条）：**

1. ❌ **删我的**「CUDA `items_done/bytes_done` 是伪造核算」—— 子代理证明执行区间恒等于 `token.size()`。**整条删除。**
2. ❌ **删我的**「`elapsed_ns` 恒为 0」—— `set_tls_elapsed` 被每个生产 launcher 调用。**整条删除。**
3. ❌ **降级我的**「`prefetch_input`（单数）slot0 挤掉」—— 子代理证明该重载**零生产调用者**，且 `:221` 的委派不可达。**降为非阻断，真实危害移交 F17。**
4. ❌ **降级我的**「`test_scheduler.cpp:190` 是自洽式断言」—— 11000 可独立重推。**撤回。**
5. ❌ **降级我的**「residency 未知键 `HostValid` 导致 fail-open」—— 零生产调用者。**降为建议。**
6. ❌ **降级我的**「`config_hot_read::read()` 是活跃 fail-open」—— 零生产实例。**降为建议，并采纳子代理新发现：该 fail-open 被测试断言成了绿。**
7. ❌ **删我的**「NaN 乘进成本」—— 唯一使用点是拼字符串。**撤回算术传播，保留"校验放行 NaN"。**
8. ❌ **删我的**「`simulate_mixed` 因首块超界而全死」—— `take ≤ back` 恒成立。**删除该子断言。**
9. ❌ **删我的**「`op_id_to_enum` 导致 weighted_integration 误评分」—— 不可达。**撤回，改列 S9。**
10. ❌ **修正我的**「uint64 下溢 ⇒ `h2d_gbps=0` ⇒ `host_path_eligible=true`」—— 子代理算出下溢后 `gbps≈3.6e-12>0`，实际使 eligibility 保持 false（**方向相反**）。**改写为：真正的零斜率路径是 `dn==0`。**
11. ❌ **否决子代理的**「StarPU `platform: Linux` 不一致」—— 它确实仅 Linux，字段正确。**不报此条。**
12. ❌ **否决子代理的**「ADR-006 与 dependency-lock 的 commit 记录不同」—— 二者对 commit **一致**（都是 v1.15.2）；真实缺口是 ADR-006 **通篇不提实际的 1.17.0**。**采纳更精确表述。**

**我采纳并升格的 5 条（全部来自子代理）：**
- **F17**（`in_set` 过期快照 ⇒ 两输入互挤 ⇒ 错误测光被上报为成功）：由 `128e1710` 发现 ⇒ **直接列为最高阻断**。这是我本人读 `cuda_bridge_loader.cpp` 时**漏掉**的。
- **F3**（`eng/tools/` 路径错 ⇒ 四个工具静默不入构建）：两个子代理独立发现，我亲自 `ls` 复核 ⇒ **升为阻断**。
- **F9**（稀疏算子曲线键永久不匹配）：两个子代理独立发现，我亲自读三处原文复核 ⇒ **升为须修**。
- **`mark_host_dirty` 漏 `BothValid`**（`residency_manager.cpp:90-95`）：我**自己读时漏掉**，由 `0713ae43` 补出。
- **`fingerprint_runtime_kernel_hash` 是装饰字段**（只查 `.size()<16`、从不与新算值比较）：把我原本的「ASLR ⇒ 比较必失败」修正为更准确的「**从不比较**」⇒ 该分支永不执行。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线与纪律
git -c core.quotepath=false rev-parse HEAD                       # 850a9edefd47434b9ab71bc907c3de1e0814b323
git -c core.quotepath=false status --porcelain -- lib/infrastructure/acr/   # 空
git -c core.quotepath=false status --porcelain -- 实验/reviews/  # ?? 实验/reviews/ ← 越权写，须删

# 1) 成员清单与行数（口径：逻辑行数）
python3 - <<'PY'
import re,os
lines=open('run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml',encoding='utf-8').read().split('\n')
s=[i for i,l in enumerate(lines) if '片号: INF-acr-004' in l][0]
blk=[]
for l in lines[s:]:
    if l.strip().startswith('- 片号:') and 'INF-acr-004' not in l: break
    blk.append(l)
fs=[m.group(1) for l in blk if (m:=re.match(r'\s*-\s+"(.*)"\s*$',l))]
print(f"members={len(fs)} missing={sum(1 for f in fs if not os.path.exists(f))} "
      f"lines={sum(1 for f in fs for _ in open(f,encoding='utf-8'))}")
PY

# 2) F17 in_set 过期快照（最高阻断）
sed -n '217,264p' lib/infrastructure/acr/backends/cuda/cuda_bridge_loader.cpp

# 3) F0 资格门恒空
sed -n '37,42p' lib/infrastructure/acr/qualification/focused/operation_profile.hpp
sed -n '369,375p;378,380p;405,415p' lib/infrastructure/acr/qualification/focused/focused_benchmark.cpp

# 4) F1 自写判据
sed -n '250,276p' lib/infrastructure/acr/tests/classic/e18_workpool.cpp

# 5) F2 先赋值再断言自己 + 门限从未被断言
sed -n '183,196p' lib/infrastructure/acr/tests/unit/test_profile_holdout.cpp
grep -n 'EXPECT_LE(median\|EXPECT_LE(p95' lib/infrastructure/acr/tests/unit/test_profile_holdout.cpp || echo "→ 门限确实未被断言"

# 6) F3 四个 CLI 工具被静默丢弃
sed -n '225,238p' lib/infrastructure/acr/CMakeLists.txt
cd lib/infrastructure/acr && for d in eng tools/acr_status tools/acr_classic_runner buffers backends/alpaka; do
  [ -d "$d" ] && echo "EXISTS  $d" || echo "MISSING $d"; done

# 7) F6 device_id 零写入 + dispatcher 硬编码
grep -n 'device_id' lib/infrastructure/acr/scheduler/residency_manager.cpp
grep -n 'residency.is_device_valid' lib/infrastructure/acr/scheduler/dispatcher.cpp   # 5 处 "cuda:0"

# 8) F8 NaN 通过校验
sed -n '34,38p' lib/infrastructure/acr/core/task_descriptor.cpp
grep -cn 'nan\|NAN\|infinity' lib/infrastructure/acr/tests/unit/test_task_descriptor.cpp || echo "→ 无 NaN 单测"

# 9) F9 曲线键永久不匹配
sed -n '63,66p' lib/infrastructure/acr/cost/cost_estimator.cpp
sed -n '451,453p' lib/infrastructure/acr/qualification/profile_generator.cpp
sed -n '97,98p'   lib/infrastructure/acr/include/astro/compute/hardware_profile.hpp

# 10) F13 parallel_scan 丢弃 kernel
sed -n '339,364p' lib/infrastructure/acr/include/astro/compute/acr.hpp

# 11) F15 锁文件没有 SHA + 与 ADR 矛盾
grep -nE '"commit"' lib/infrastructure/acr/docs/dependency-lock.json
grep -nE '[0-9a-f]{40}' lib/infrastructure/acr/docs/dependency-lock.json || echo "→ 零个 40 位 SHA"
grep -n '1\.15\.2\|1\.17\.0' lib/infrastructure/acr/docs/ADR-006-googletest.md

# 12) S12 两个 sanitizer 开关同名不同名 / S13 windows.h 守卫
grep -rn 'ACR_BUILD_SANITIZER\|ACR_ENABLE_SANITIZER' lib/infrastructure/acr --include=CMakeLists.txt
grep -n 'windows.h\|始终编译' lib/infrastructure/acr/backends/cuda/cuda_bridge_loader.cpp lib/infrastructure/acr/backends/cuda/CMakeLists.txt
```

---

## 9. 交付说明

- **本文件是本轮唯一交付物**：`run/GOVERN-08/审核包-R2/审稿-P1-INF-acr-004.md`
- **未做**：零 git 写、未编译、未跑 ctest/pytest/构建/任何二进制、未读 `/tmp/acsd_g08/`、未改任何仓内被审文件。
- **需负责人处置**：删除子代理越权写入的 `实验/reviews/g08_residency_config_traits_review.md`。
- **本片判定：阻断。** F17 是最高优先级——它不在性能域，而在**科学结果正确性**域：加权积分的帧数组会被权重数组覆盖，而状态是 `Ok / all_done=true` 且被记为 GPU 执行。F0 位于资格认证主链；F1/F2 摧毁的是"泛化能力已被验证"这一前提；F3 使本片 2 个成员与整条经典实验链不可构建。