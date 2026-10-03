# 对抗式复核：residency_manager / config_hot_read / task_descriptor

HEAD 850a9ede。只读复核，未修改任何被审文件。

---

## CLAIM 1 — residency_manager.{hpp,cpp}

### (a) device_id 永不被记录 —— **CONFIRMED（并强化）**

- `mark_uploaded(const std::string& key)` 无 device 参数：cpp:105；声明 hpp:82。全仓无第二个重载。
- `BufferResidency::device_id{"cuda:0"}` 默认值：hpp:43。
- **全仓对 `BufferResidency::device_id` 的引用只有 3 处，全部是读**：
  cpp:149（`is_device_valid` 比较）、cpp:160（`needs_upload` 比较）、cpp:211（`status_json` 打印）。
- 无 setter，无 `b.device_id = ...`，无 `kv.second.device_id = ...`（对 `device_id\s*=` 全仓 grep 的命中全部属于 `DeviceProfile::device_id`（int 型，hardware_profile.hpp:177）、`DeviceCost::device_id`（cost_estimator.hpp:39）、`PerDeviceStats::device_id`（dispatcher.hpp:165）、`FocusedOp::gpu.device_id`（operation_profile.hpp:58）等其它类型）。
- 结论：**没有任何代码路径记录实际上传设备**，该字段恒为 `"cuda:0"`。

**强化（你漏掉的、比 (a) 本身更严重的一点）**：生产侧根本没有"正确传 device id"的可能——
`dispatcher.cpp` 五处调用**全部硬编码字符串字面量 `"cuda:0"`**：
- dispatcher.cpp:1857 `is_device_valid(key, "cuda:0")`
- dispatcher.cpp:1880 `is_device_valid(key, "cuda:0")`
- dispatcher.cpp:2165 `is_device_valid(key, "cuda:0")`
- dispatcher.cpp:2194 `is_device_valid(key, "cuda:0")`
- dispatcher.cpp:2227 `is_device_valid(key, "cuda:0")`

即使修好记录端，查询端仍写死 cuda:0，`device_id` 参数在本模块内是死参数。

### (b) 反例是否成立 —— **PARTIALLY（API 层成立，生产层后果与你的描述不同）**

逐步推演 `mark_uploaded("frames")` 后真实上传发生在 cuda:1：

1. `mark_uploaded` → cpp:108-110：state 由 `HostValid` 走 else 分支 ⇒ `BothValid`；`device_id` 仍 `"cuda:0"`（hpp:43 默认）。
2. `is_device_valid("frames","cuda:1")` → cpp:149 `"cuda:0" != "cuda:1"` ⇒ **false**。✔ 与你说的一致。
3. `is_device_valid("frames","cuda:0")` → cpp:149 相等，cpp:150-151 `BothValid` ⇒ **true**。✔ 与你说的一致（假阳性）。
4. `needs_upload("frames","cuda:1")` → cpp:160 不等 ⇒ **true**，且因 cpp:158/160 在 state 检查之前返回，**永远 true**（state 变 BothValid 也无用）。✔ 与你说的一致。

三点全部成立。**但你描述的后果需要修正**：
`needs_upload` / `is_device_valid` 的 `device_id` 参数在生产中**从未被真实设备查询过**（见上，(a) 强化），所以"cuda:1 上永久重复上传"在今天的生产路径上**不会发生**——所有查询都是 `"cuda:0"`。
第 3 点的**假阳性**才是真正的危害：`is_device_valid` 返回 true 会让 dispatcher.cpp:1880 **跳过上传**、让 dispatcher.cpp:2194 把 `was_resident` 置 true、进而让 dispatcher.cpp:2206-2236 得出 `data_resident`/`persistent_resident`，最终作为 BDR 路由事实喂给 dispatcher.cpp:2257-2259 的 `req.input_residency`。

### (c) 三个 unknown-key 默认值 + 是否 fail-open —— **REFUTED（你担心的 fail-open 不可达）**

实测三个默认值：
- `state(unknown)` = `HostValid`（cpp:176-177）
- `needs_upload(unknown, dev)` = `true`（cpp:158）
- `needs_download(unknown)` = `true`（cpp:168）

**生产消费者全量清点**（全仓 grep，排除 tests）：

| 方法 | 生产调用点 |
|---|---|
| `is_device_valid` | dispatcher.cpp:1857, 1880, 2165, 2194, 2227（**仅此 5 处**） |
| `register_or_update` | dispatcher.cpp:1858, 2166 |
| `recorded_generation` | dispatcher.cpp:1855, 2163 |
| `mark_uploaded` | dispatcher.cpp:868, 1898, 2481 |
| `mark_device_allocated` | dispatcher.cpp:869, 1899, 2482 |
| `mark_downloaded` | dispatcher.cpp:2521 |
| `state` | **无** |
| `needs_upload` | **无** |
| `needs_download` | **无** |
| `register_buffer` | **无** |
| `mark_host_dirty` | **无** |
| `mark_device_dirty` | **无** |
| `generation` | **无** |
| `is_device_allocated` | **无** |
| `upload_count` / `download_count` | **无** |
| `total_uploads` / `total_downloads` | **无** |
| `status_json` | **无** |

**结论**：
- `state()` 的 `HostValid` 默认 **今天无法造成任何传输被跳过**——生产代码根本不调用 `state()`，只调用 `is_device_valid`（unknown 时 cpp:147 明确 `return false`，安全）。
- 三个 unknown 默认在"保守"语义上彼此一致：都假定"什么都没有驻留"。
- 唯一的不一致：`state(unknown)=HostValid` 与 `needs_download(unknown)=true` 在语义上打架（host 有效就不需要下载），但都是保守方向，不构成 fail-open。
- **这是一个潜伏陷阱而非活跃缺陷**：一旦有人写 `if (state(k) == HostValid) skip_upload()` 就会 fail-open。正确修法是把 unknown 变成独立的 `ResidencyState::Unknown` 或让 `state()` 返回 `std::optional<ResidencyState>`。

### (d) "报告实际传输字节" —— **CONFIRMED 是悬空声明（DANGLING）**

hpp:6-8 头注释声称"报告实际传输字节与驻留复用次数"；hpp:108 注释"报告：该 buffer 的实际传输次数与字节"。

- `BufferResidency`（hpp:41-51）有 `bytes`（hpp:44）、`upload_count`（48）、`download_count`（49），**没有任何"已传输字节"累加器**。
- `status_json()`（cpp:200-222）输出 `"bytes":kv.second.bytes`（cpp:212）——这是 `register_or_update` 里 caller 传入的**缓冲区容量**（dispatcher.cpp:2148 `binding.count * binding.element_size_bytes`），**不是传输量**；`"uploads"/"downloads"`（cpp:213-214）是次数。
- 真正的传输字节统计在 **ResidencyManager 之外**：`TransferStats::h2d_bytes` / `d2h_bytes`（dispatcher.hpp:185-186），赋值于 dispatcher.cpp:2501 与 dispatcher.cpp:2587。
- 判定：**在该模块内是悬空声明**（且 `upload_count()`/`download_count()` 返回计数而非字节，与 hpp:108 注释字面不符）。`status_json()` 本身在生产中亦无调用者。

### (e) generation 回退 —— **PARTIALLY CONFIRMED（回退成立；manager 自身不 fail-open，fail-open 在消费端）**

- **回退成立**：`mark_host_dirty` 在 cpp:96 做 `++b.generation`（本地自增，与外部 binding generation 计数器无关）；`register_or_update` 在 cpp:77 做无条件 `b.generation = generation`（外部值直接覆盖）。故 manager 记录值可增可减。
- **cpp:68 用 `!=` 而非 `>`**：hpp:66 注释写"同 key 但 generation **高于**已记录"，实现是"不等于"。故**回退会导致假失效**（多余 H2D + 复用不稳定），方向是 fail-closed，不危险。
- **manager 自身不会把过期 device 副本判为有效**：我穷举了所有状态迁移，`mark_host_dirty`（cpp:90-95）在 `BothValid` 上落到 else 分支置 `HostValid`，`is_device_valid` 随即返回 false——自洽。
- **但消费端会 fail-open**，具体交错：

```
① Dispatch#1: register_or_update(key, N, Read, 0)   cpp:61-77 → b.generation=0
② prefetch_inputs 真实上传                          bridge:248-261 → views_[host]=bytes, slot_host_[0]=host
③ mark_uploaded(key)                                cpp:108-110 → state=BothValid, b.generation=0
④ host 原地改写内容，调用 mark_host_dirty(key)        cpp:90-92 走 else → state=HostValid
                                                 cpp:96  ++b.generation → b.generation=1  ← 超过 caller 的 0
⑤ Dispatch#2，caller 正确地把 binding.generation 0→1
   dispatcher.cpp:2163  old_gen = recorded_generation = 1        ← 不是 0
   dispatcher.cpp:2165  had_device_copy = false（state=HostValid，cpp:150-151）
   dispatcher.cpp:2166  register_or_update(key,N,Read,1)
                        cpp:68 `1 != 1` 为 false → 不触发失效分支
   dispatcher.cpp:2168  `binding.generation(1) != old_gen(1)` 为 false
                        → invalidate_input(binding.data) 【不被调用】
⑥ 于是每个 CudaExecutor handle 的 views_ 仍保留该 host 映射（bridge:260 未被撤销）
⑦ dispatcher.cpp:2194 is_device_valid=false → 该输入进入 input_hosts
⑧ dispatcher.cpp:2477 prefetch_inputs → bridge:235
      `if (views_.find(hosts[i]) != views_.end()) continue;`  ← 真实 H2D 被跳过
⑨ dispatcher.cpp:2479-2481 `!was_resident[i]` 成立
      → mark_uploaded 被调用；h2d_bytes_this += input_bytes[i]（dispatcher.cpp:2480）
      → 报告声称"已传输 N 字节"，实际一个字节都没传
⑩ dispatcher.cpp:885 `inv.input_resident = true;`（无条件置真）
   weighted_integration_kernels.cpp:125-129 → 走 submit_weighted_integration_resident
   bridge:715-739 该分支不重传 x，直接用 h->d_x + begin 计算 → 读到 ④ 之前的旧内容
```

**判定**：回退是真的；用它做 fail-open 的不是 manager 而是 `dispatcher.cpp:1860/2168` 的 `invalidate_input` 门 + `bridge:235` 的 `views_` 短路。
**诚实限定**：`mark_host_dirty` 在生产中**无调用者**（仅 tests/unit/test_residency.cpp:52,103），所以这条交错今天需要外部代码主动调用该公开 API（hpp:76）才可达。上线风险是"接口已公开、一接即中"。

---

## CLAIM 2 — config_hot_read.{hpp,cpp}

### (a) `read()` 静默默认 —— **CONFIRMED 存在，但零生产影响（该类根本没有生产实例）**

- `read()` cpp:63-65：`initialized()==false` ⇒ `return HotConfig{}`，无异常、无错误码、无日志。
- 默认值全部"看起来合理"：ram_ratio 0.95 / vram_ratio 0.95（cpp:17-18）、reserve 2GiB / 512MiB（cpp:19-20）、fallback_policy `BestEffort`（cpp:21 + hpp:43）。
- **调用者清点（关键发现）**：`ConfigHotReader` 全仓仅出现在 config_hot_read.{hpp,cpp}、CMakeLists.txt:15、以及 **tests/unit/test_utilization.cpp:218,242,260,277,285**。**无任何生产调用者**，`read()` 亦然。
- 判定：API 层面确是 silent-default fail-open（尤其 0.95 比例 + BestEffort，见 (e)），但**当前无生产路径可触发**。严重性应从"活跃缺陷"降为"未接线组件的接口缺陷"。

### (b) `read()` 是否真为"线程安全快照" —— **CONFIRMED 与 hpp:72 矛盾**

- hpp:71-72："// 读取当前配置（线程安全快照）"。
- cpp:67-71：**四个热字段在 mutex 之外**用四个**互相独立**的 `memory_order_relaxed` 原子逐个 load；`std::lock_guard` 只在 **cpp:74** 才取得，仅保护 `max_threads/gpu_backend/isa_level/backend_enabled`。
- 具体交错（与 `update_hot`）：
```
初值 (gen N): ram_ratio=0.5, vram_ratio=0.9, ram_reserve=1GiB, vram_reserve=256MiB
线程A read():  cpp:67  load ram_ratio  → 0.5     ← 读到旧值
线程B update_hot(gen N+1 {ram_ratio=0.9, vram_ratio=0.5, 4GiB, 512MiB})
              cpp:55-59 store 0.9 / 0.5 / 4GiB / 512MiB   （持锁，但读者此刻未持锁）
线程A read():  cpp:68  load vram_ratio → 0.5     ← 读到新值
              结果 = {ram_ratio 0.5, vram_ratio 0.5, ...} —— 任何一代配置中都不存在的组合
```
- **重要修正**：`init()` 路径**没有**这个问题——cpp:30-34 先写四个原子，cpp:35 才 `initialized.store(true, release)`，配合 cpp:63 的 acquire load，构成正确的 release/acquire 发布，读者看到 initialized==true 即保证四个原子可见。撕裂**只发生在 `update_hot` 期间**。
- 补充：`set_ram_ratio` 等（cpp:83-87）把原子 store 放在**取锁之前**（cpp:84 先 store，cpp:85 才 lock），同样制造一个"原子已新、冷字段仍旧"的观察窗口。
- 判定：cpp:67-71 与 cpp:83-105 缺少 seqlock / 版本号 / 单一锁域，hpp:72 的"快照"承诺**不成立**。

### (c) 六个 setter 的 `noexcept` —— **CONFIRMED，六处全部声明 noexcept**

hpp:75 `set_ram_ratio`、hpp:76 `set_vram_ratio`、hpp:77 `set_ram_reserve`、hpp:78 `set_vram_reserve`、hpp:79 `set_fallback_policy`、hpp:82 `set_backend_enabled` —— 六处签名末尾均有 `noexcept`（已逐行确认）。

实现均在函数体内取 `std::lock_guard<std::mutex>`：cpp:85、cpp:91、cpp:97、cpp:103、cpp:109、cpp:114。`std::mutex::lock()` 可能抛 `std::system_error` ⇒ `noexcept` 函数内抛出即 `std::terminate`。

**对抗性加强（你漏掉的、比 mutex 更高频的一条）**：`set_backend_enabled` 在 cpp:115 执行 `impl_->cfg.backend_enabled[backend] = enabled;`——`std::unordered_map::operator[]` 需要**分配节点**，可能抛 `std::bad_alloc`。同样是 `noexcept`（cpp:113 / hpp:82）⇒ 直接 `std::terminate`。且 `set_backend_enabled` 是六个 setter 中**唯一没有对应热原子**的（Impl 的五个原子 cpp:17-21 不含 backend_enabled），说明作者本意就是"短暂持锁"，`noexcept` 更像是随手加的。

判定：是真实的 terminate 隐患（尽管实践中 std::mutex 构造失败概率极低、bad_alloc 概率非零）。

### (d) `cold_frozen` 死状态 —— **CONFIRMED 死状态**

- 声明 cpp:14 `std::atomic<bool> cold_frozen{false};`，**全仓唯一一次写**是 cpp:36 `impl_->cold_frozen.store(true, std::memory_order_release);`。
- 全仓 grep `cold_frozen` 共 2 处命中，都在 config_hot_read.cpp:14 与 :36。**无任何 read**。
- 对照 hpp:45「---- ColdStatic（仅启动时设，运行时冻结）----」与 hpp:57「ColdStatic 项仅启动时设置后冻结」：**冻结语义没有由 `cold_frozen` 实现**，而是由"根本不写这些字段"偶然实现的——`update_hot`（cpp:46-54）确实不碰 `max_threads/gpu_backend/isa_level`，但那是**靠代码作者自律**，不是靠 `cold_frozen` 标志。任何人加一条写冷字段的路径都不会被这个标志拦住。
- 判定：**伪装成冻结保证的死状态**，误导性注释来源。

### (e) `FallbackPolicy` 默认 `BestEffort` —— **CONFIRMED fail-open，且比你说的更糟：这是一个和生产策略完全脱钩的影子枚举**

- 默认值 hpp:43 `FallbackPolicy fallback_policy{FallbackPolicy::BestEffort}` + cpp:21 原子初值一致。
- **致命发现**：仓库里存在**两个互不相通的回退枚举**：
  - `astro::compute::utilization::FallbackPolicy`（config_hot_read.hpp:24-29）：`Strict=0 / PreferCpu=1 / PreferOtherGpu=2 / BestEffort=3`
  - `astro::compute::scheduler::FallbackStrategy`（fallback.hpp:21-25）：`None=0 / ToCpu=1 / ToNextDevice=2`
  - 两者**没有任何转换函数、没有映射表**。
- 真正生效的是后者：`FallbackPolicy` 类（fallback.hpp:36-55，`strategy_{FallbackStrategy::ToCpu}` 见 fallback.hpp:54）、由 `Dispatcher::configure` 装配（dispatcher.cpp:1796 `impl_->fallback_policy.set_strategy(cfg.fallback_strategy)`），来源是 `DispatcherConfig::fallback_strategy`（dispatcher.hpp:50），由 runtime.cpp:549 设置。
- 判定：config_hot_read 的 `FallbackPolicy` 默认值不只是"fail-open"——它是**一整套永远不生效的配置表面**。即使有人调用 `ConfigHotReader::set_fallback_policy(Strict)`（cpp:107-111），dispatcher 的回退行为也**完全不变**。`Strict`（严格：无可用 backend 即失败）这个项目里唯一 fail-closed 的选项从未接入生产。
- 附带：`is_backend_enabled`（cpp:118-125）unknown backend 返回 `true`（cpp:122 注释"未配置默认启用"），也是默认启用方向的 fail-open。

---

## CLAIM 3 — task_descriptor.cpp:35-38

### NaN 通过校验 —— **CONFIRMED**

- cpp:36 `if (t.active_fraction_hint < 0.0 || t.active_fraction_hint > 1.0) return false;`
- IEEE-754：`NaN < 0.0` 为 false，`NaN > 1.0` 为 false ⇒ 析取为 false ⇒ **cpp:37 `return true`**，NaN 被判为**有效**。
- 修法应为 `if (!(t.active_fraction_hint >= 0.0 && t.active_fraction_hint <= 1.0)) return false;`（或 `std::isfinite`）。

### 「NaN 被乘进成本/工作量估算」 —— **REFUTED（你要的证据不存在）**

- `active_fraction_hint` 全仓**仅 1 处生产使用**：`cost_estimator.cpp:64`
  `os << "gather:random:" << task.traits.active_fraction_hint;`
  这是**拼进一个字符串查找键**，不是数值运算。
- 该键在 `estimate_compute_cost` 里用于 `dev.get_curve(lk.family, lk.key)`（cost_estimator.cpp:174，`CapabilityFamily::Irregular` 分支）。命中失败 ⇒ `curve_cost` 保持 0.0 ⇒ 落到 **205-211 行的峰值带宽兜底**（`bytes = work*8` 之类），返回的是有限正数。
- 结论：**NaN 不会传播成 NaN 成本**，它只是让曲线查找 miss，从而降级到通用带宽模型。所以你要求的"乘进成本估算"的证据**我找不到，也不应声称存在**。

### 单测是否喂 NaN —— **CONFIRMED 没有**

`tests/unit/test_task_descriptor.cpp` 中 `task_traits_valid` 的全部用例只有 5 个断言：
- :170 `EXPECT_TRUE(task_traits_valid(t))`（默认值 1.0）
- :175-176 `= 1.5` → EXPECT_FALSE
- :177-178 `= -0.1` → EXPECT_FALSE
- :183-184 `= 0.0` → EXPECT_TRUE
- :185-186 `= 1.0` → EXPECT_TRUE
**无 NaN、无 ±inf。**

### 函数名是否过度承诺 —— **CONFIRMED 过度承诺（mis-scoped）**

- 名字 `task_traits_valid` 承诺"整个 TaskTraits 合法"，实现（cpp:35-38）只检查一个 double。
- 实证：schema `task_descriptor.schema.json:118-127` 为 **两个** 数值字段规定了 `minimum:0, maximum:1`：`active_fraction_hint`（:118-122）**和 `atomic_contention_hint`（:123-127）**。后者**完全不被校验**。
- 而 `atomic_contention_hint` 在 `TaskTraits`（task_traits.hpp:135-150）里**根本没有这个字段**——schema 描述了一个 C++ 结构不存在的成员。
- 其余未校验项：task_class / access / uniformity / intensity（hpp:136-139）、`splittable`/`mixed_device_safe`/`requires_atomic`（hpp:142-144）、halo_x/halo_y（hpp:148-149）、`bytes_read_per_item`/`bytes_written_per_item`（hpp:146-147）。
- 缓解事实：header 注释 task_descriptor.hpp:133 写的是「TaskTraits 默认值校验（测试/诊断用）：active_fraction_hint 必须在 [0, 1]」——注释比函数名诚实。**真正误导的是函数名，不是注释。**
- 同样缓解：`task_traits_valid` 全仓**只有测试调用**（test_task_descriptor.cpp:170,176,178,183,186 + 声明 task_descriptor.hpp:134 + 定义 cpp:35），**无生产调用者**，所以当前无生产 fail-open。

---

# NEW DEFECTS I FOUND（你漏掉的）

按严重性排序。每条都基于我亲自读到的行。

## N1（高）— Irregular 曲线键 生产端/消费端 永久不匹配，稀疏算子成本模型整体失效
- 写端：`profile_generator.cpp:452` `auto& c = device.irregular["gather:random"];` —— **无分数后缀**
- 读端：`cost_estimator.cpp:64` `os << "gather:random:" << task.traits.active_fraction_hint;` —— **恒有后缀**
- 文档：`hardware_profile.hpp:98` `// irregular: "gather:random:0.05" / ...` —— **有后缀**（文档站消费端，不站生产端）
- 后果：`dev.get_curve(Irregular, key)`（cost_estimator.cpp:174）对**任何** `active_fraction_hint` 取值（0.0 / 0.5 / 1.0 / NaN）都必然 miss ⇒ `curve_cost` 保持 0 ⇒ 落到 cost_estimator.cpp:205-211 的峰值带宽兜底，`used_profile_curve` 永远 false。
- 加重项：全仓 `--include=*.json` grep `gather:random` **零命中**，即没有任何已落盘的 profile 含该曲线。
- 语义后果：`active_fraction_hint`（task_traits.hpp:145「active 比例（sparse 任务 0.01~0.5）」）对成本**完全无影响**——稀疏度只当查找键用，稀疏任务与稠密任务用同一条带宽模型。这比 NaN 问题严重得多，且与 NaN 无关。

## N2（高）— `BufferResidency::device_id` 恒为 `"cuda:0"` ⇒ 多 GPU 下把"某卡有副本"当成"所有卡都有副本"
- 事实链：`BufferResidency::device_id` 恒定（hpp:43，全仓无写）；查询全写死 `"cuda:0"`（dispatcher.cpp:1857,1880,2165,2194,2227）。
- 但实际上传**只投给第一张卡**：`dispatcher.cpp:2475-2476` 遍历 `available_executors()` 取第一个 cuda backend，`dispatcher.cpp:2486` `break;`。
- 而工作**分发给所有卡**：`dispatcher.cpp:963-1008` 把每个 eligible cuda executor 放进 `supported`；`dispatcher.cpp:1072` `n_exec = supported.size()`；每个 executor 各自领块（dispatcher.cpp:1673, 1714）；`dispatcher.cpp:1732,1745` 按 `supported[i]->device_id()` 分别记账。
- 后果：`dispatcher.cpp:2194` 对 2..N 号卡返回 `was_resident=true`，`dispatcher.cpp:2206-2236` 得出 `data_resident=true`，`dispatcher.cpp:2257-2259` 以 `InputResidency::DeviceResident` 作为 BDR 路由事实；而那些卡的 `d_x` 槽（`acr_cuda_bridge_host.cpp:704-707`、每个 handle 私有）从未收到过 H2D。
- `dispatcher.cpp:885` 无条件 `inv.input_resident = true;` ⇒ `weighted_integration_kernels.cpp:125-129` 选 `submit_weighted_integration_resident`（bridge:715-739 不重传 x）⇒ **读到未初始化的 d_x**。
- 判定：这是 (a)/(b) 在生产中的**真实危害形态**，比"永久重复上传"严重一个量级（静默错误结果，非性能问题）。

## N3（中）— ResidencyManager 与 executor 本地 `views_` 是两份事实源，传输字节统计会失真
- `cuda_bridge_loader.cpp:260` `views_[hosts[i]] = bytes;`（executor 私有、按 host 指针 key）
- `cuda_bridge_loader.cpp:235` `if (views_.find(hosts[i]) != views_.end()) continue;`（按 `views_` 短路，非按 manager）
- `cuda_bridge_loader.cpp:268-275` `invalidate_input` 是**唯一**能清 `views_` 的入口，且只被 dispatcher.cpp:1864 / 2172 在门控下调用。
- 而 dispatcher.cpp:2479 / 866 的记账条件是 `!was_resident[i]`（来自 manager 的 `is_device_valid`）。两者可以不一致：
  - manager 说 resident（`was_resident=true`）但 executor 的 `views_` 被清 ⇒ `mark_uploaded` 不被调用、`h2d` 不被计数（dispatcher.cpp:866-870），但 `prefetch_inputs` **实际传了** ⇒ `transfer_stats.h2d_bytes`（dispatcher.cpp:2501）**低报**真实传输量。
  - 反向即 CLAIM 1(e) 的 ⑨：**高报**。
- 判定：`hpp:8` 承诺的"报告实际传输字节"在系统层面也不成立，因为计数依据和实际动作依据是两个独立状态机。

## N4（中）— `task_descriptor.schema.json` 描述了 C++ 结构不存在的字段
- schema:123-127 定义 `atomic_contention_hint`（number, 0..1）
- `TaskTraits`（task_traits.hpp:135-150）**没有** `atomic_contention_hint` 成员
- ⇒ schema 校验通过 ≠ C++ 侧存在该字段；任何按 schema 生成的配置在 C++ 侧被静默丢弃。

## N5（低）— `update_hot` 只能合并、不能删除 backend 条目
- config_hot_read.cpp:52-54 `for (const auto& kv : cfg.backend_enabled) { impl_->cfg.backend_enabled[kv.first] = kv.second; }`
- 只 insert/overwrite，**没有 erase**。已写入的条目永久留在 map 里；配合 `is_backend_enabled` 的 unknown→true 默认（cpp:122），一旦某 backend 曾被显式置 true 就再也回不到"未配置"语义。

## N6（低）— `update_hot` 会绕过"启动期 ColdStatic"边界
- config_hot_read.cpp:40-43：`if (!initialized.load(acquire)) { init(cfg); return; }`
- 即"热更新"接口在未初始化时会**隐式执行初始化**，把 `initialized` 置真（cpp:35），使得 `read()` 的 silent-default 分支（cpp:63-65）被绕过，且 ColdStatic（`max_threads`/`gpu_backend`/`isa_level`）被热更新调用方设定——与 hpp:57「ColdStatic 项仅启动时设置后冻结」矛盾。
- 这正是 (a) 那个 fail-open 的**唯一实际修复入口**：任何人在 `init()` 之前误接一个 `update_hot` 就能把非法配置变成"已初始化"。

## N7（低）— `update_hot` 的 `backend_enabled` 合并与 `set_backend_enabled` 不对称
- `update_hot` 在 cpp:52-54 合并 `backend_enabled`，但它的热原子（cpp:17-21）**不含** backend_enabled；`set_backend_enabled`（cpp:113-116）同理。
- 结果：`read()`（cpp:78）在锁内读 `backend_enabled`，`status_json()`（cpp:154）也在锁内读——这条是一致的。**但** `read()` 返回的 `HotConfig.backend_enabled` 是**快照拷贝**（cpp:78 `out.backend_enabled = impl_->cfg.backend_enabled;`），调用方拿到的 map 与后续热更新无关。若调用方据此缓存判断，会拿到过期快照。（当前无生产调用者，故仅为接口层瑕疵。）

---

# 一句话汇总

| 子项 | 判定 |
|---|---|
| 1(a) device_id 永不被记录 | **CONFIRMED**（并强化：查询端亦全写死 "cuda:0"） |
| 1(b) 反例 | **PARTIALLY**（API 层四点全成立；生产危害是假阳性静默错值，非"永久重传"） |
| 1(c) unknown-key 三默认 | **REFUTED**（无 fail-open：`state()` 零生产调用者；仅潜伏陷阱） |
| 1(d) 报字节 | **CONFIRMED 悬空**（只有容量与计数，无传输字节累加器） |
| 1(e) generation 回退 | **PARTIALLY**（回退成立；manager 自身 fail-closed，fail-open 在 dispatcher+bridge；需 mark_host_dirty，该 API 当前无生产调用者） |
| 2(a) read() 静默默认 | **CONFIRMED 存在，零生产影响**（ConfigHotReader 无任何生产实例） |
| 2(b) 非原子快照 | **CONFIRMED**（仅 update_hot 路径；init 路径 release/acquire 正确） |
| 2(c) noexcept terminate | **CONFIRMED**，六处齐全；更高频路径是 cpp:115 map 分配 bad_alloc |
| 2(d) cold_frozen 死状态 | **CONFIRMED**，全仓 2 处命中（声明+单次写），零 read |
| 2(e) BestEffort 默认 fail-open | **CONFIRMED，且更糟**：是与生产 `FallbackStrategy` 完全脱钩的影子枚举 |
| 3 NaN 通过校验 | **CONFIRMED** |
| 3 NaN 乘进成本 | **REFUTED**（唯一使用点 cpp:64 只是拼字符串键） |
| 3 无 NaN 单测 | **CONFIRMED**（仅 1.5 / -0.1 / 0.0 / 1.0 / 默认） |
| 3 命名过度承诺 | **CONFIRMED**（schema 还有第二个 [0,1] 字段未校验，且该字段 C++ 侧不存在） |