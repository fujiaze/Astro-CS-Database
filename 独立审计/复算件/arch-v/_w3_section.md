
---

## W3 · 唯一执行器池靠文本包含走私 ＋ 生产里私建线程池 ＋ 第二资源预算源

**判定：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低）**

### 我的独立取证

工具：`独立审计/复算件/arch-v/w3_targets.py`（**只读 CMake 源文本**推构建图归属：解析全跟踪 `CMakeLists.txt`/`.cmake` 的 `add_library/add_executable` 源清单 + `target_link_libraries` 链接闭包 + `install(TARGETS)` 白名单；再叠加"被编译 TU 的引号 `.cpp/.inc` 文本包含"传递闭包。零 cmake 调用）。

**(a) `#include "*.cpp"` 文本包含全仓共几处**（命令：`git grep -nE '#[[:space:]]*include[[:space:]]*"[^"]+\.(cpp|cc|cxx|inc)"'`）

| 形态 | 生产侧 | 测试侧 | 合计 |
|---|---|---|---|
| `#include "X.cpp"` | **1** | 4 | **5** |
| `#include "X.inc"`（内核体复用惯用法，是另一件事） | 8（4 个 backend TU × 2） | 1 | 9 |
| Python 里的字符串断言（不是真包含，须排除） | — | 10 | 10 |

- 生产侧唯一一处：`lib/infrastructure/scheduler/src/module_adapters.cpp:255  #include "executor.cpp"`。
- 测试侧 4 处：`eng/tests/unit/drizzle_adapter_test.cpp:22`、`eng/tests/unit/p1_hips/adapter_entry_impl.cpp:12`、`eng/tests/unit/p1_noise/adapter_entry_impl.cpp:6`、`lib/infrastructure/aio/tests/p1hips/p1hips_digest_verify.cpp:25`。

**(b) `lib/infrastructure/scheduler/src/executor.cpp` 的构建归属（四档术语分开说，防串轴）**：

1. **不在核心库源清单**：`awk 'NR>=372&&NR<=418' CMakeLists.txt | grep -c executor` = **0**；根 `CMakeLists.txt` 全文 `grep -c "scheduler/src/executor"` = **0**。
2. **被一个测试 target 直接编译**：`eng/tests/unit/CMakeLists.txt:242-246` 的 `executor_provider_race_test` 显式列出它（`:243`）。
3. **在生产二进制里**：经 (a) 的文本包含进入 `module_adapters.cpp` ⇒ 编入 `astrocs_module_adapters`（`CMakeLists.txt:1043-1044`）⇒ 链进 `acsd`（`:1092-1093` → `:1009`）。
   ⇒ 正确表述是 **"不在生产 target 的源清单（被测试 target 编译）" ∧ "在生产链接闭包里（文本包含带入）"**；它**不是**"不被任何 target 编译"，也**不是**"已退役"。
4. **仓库自认是白名单例外**：`eng/tests/unit/CMakeLists.txt:1479-1481`「executor 实现经 astrocs_module_adapters 编入（RT-001 白名单接线…）本目标**不得再单独编** `lib/infrastructure/scheduler/src/executor.cpp`（**防重定义**）」⇒ 重定义风险已知、且只靠注释约束，无机器门。

**(c) 私建 `std::vector<std::thread>` 池逐条列位**（命令 `git grep -n "std::vector<std::thread>"`，脚本 `w3_pools.py` 按 (b) 的归属分桶；只算"声明池变量"的真 DECL，排除注释行）

生产编译 **18** 处 DECL，其中 `executor.cpp` 自身 **2** 处 ⇒ 私建 **16** 处：

| 归属 | 位置 | 池变量 |
|---|---|---|
| **算法模块内 6** | `lib/algorithms/coverage/src/sampler.cpp:934` | pool |
| | `lib/algorithms/coverage/src/upm.cpp:620, 751, 794, 916, 2143` | pool ×5 |
| **基建内 10** | `lib/infrastructure/scheduler/src/scheduler.cpp:373` | pool（通用调度器自身） |
| | `lib/infrastructure/scheduler/src/normalize_workflow.cpp:462` + `:464` | pool + prefetch_pool |
| | `lib/infrastructure/scheduler/src/mosaic_window.cpp:271` | pool |
| | `lib/infrastructure/scheduler/src/export_stream.cpp:511` | pool |
| | `lib/infrastructure/scheduler/src/module_adapters.cpp:2052, 9054` | pool ×2 |
| | `lib/infrastructure/cli/commands.cpp:1260` | hash_pool（产物哈希并发） |
| | `lib/infrastructure/benchmark/backend_host/baseline_kernels_impl.inc:72` | ths（被 4 个生产 backend TU 文本包含） |
| | `lib/phase3_session/p3_session.cpp:335` | pool（会话层，本应退役） |
| **executor 自身 2（设计指定池，不计私建）** | `executor.cpp:39`（唯一共享池）、`:217`（有界 I/O 池） | workers ×2 |

不该计入生产的池点（另 5 处，列出以免误算）：`lib/infrastructure/acr/scheduler/dispatcher.cpp:1174`（ACR 不链进 `acsd`，`CMakeLists.txt:972` 注释自证）、`lib/infrastructure/benchmark/cpu/{avx2,avx512,baseline}/src/*_provider.cpp:266/265/445`（三 provider TU 不被任何 CMake 源清单命名，见 W4）、`lib/algorithms/integration/v6/oracle/weight_chain_selfcheck.cpp:479`（oracle）。`*/tests/` 下另有 15 处（不计）。

**(d) 数量注入面 vs 池创建面（决定风险性质）**：数量确实单源——生产 `ctx.acquire_lease` 共 **4 处**（`module_adapters.cpp:598, 13139, 13349, 15270`）→ `cap` → `doc["__workers"]=cap`（`:13183`、`:13403`）→ `sc.cpu_workers = std::max(1, doc.value("__workers",1))`（`:9388`，注释"恒最后赋值"）、`uc.cpu_workers`（`:9588`）；`upm.cpp:608`/`sampler.cpp:925` 注释自陈"无 hardware_concurrency（模块不得自行开线程）"。⇒ **没有一处把线程数写死**，AGENTS §6「不硬编码线程/ISA/block：由 benchmark 生成的 profile 决定」按字面**不覆盖本形态**；被碰的是 §8.3:607 与 §9:673 的另一半。

**(e) `hardware_concurrency()` 作第二预算源的可达性（这条压低成稿的理由）**

- 生产侧唯一数值源点：`lib/algorithms/coverage/include/astro/phase2/execution_options.h:24`（`default_cpu_workers()`；`:16` `cpu_workers=0 => max(1,hardware_concurrency)`；`:38-41` `default_execution_options()`）。
- 全仓 `default_execution_options()` 的**非测试**调用者只 1 处：`lib/algorithms/coverage/include/astro/phase2/stage2_common.h:34`（`P2Stage2Config::exec` 成员默认值）。
- `P2Stage2Config` 的读者只有 `lib/algorithms/coverage/src/stage2_common.cpp` 与 `lib/algorithms/coverage/tools/stage2.cpp:710,862,1544`；后者编成 `astrocs-stage2`（`lib/algorithms/coverage/CMakeLists.txt:206`），**`EXCLUDE_FROM_ALL TRUE`**，其 `:204-205` 注释明写"compatibility 工具…**不进入任何 install/发布树**"。
- `module_adapters.cpp:97` 虽 include `stage2_common.h`，只为取 `P2_SMOOTHING_LAMBDA_AUTO` 常量（同行注释自证），**不构造 `P2Stage2Config`**。
- ⇒ 成稿点名的"第二预算源"**不在 `acsd` 生产路径上**，是工具/测试面可达的潜在第二源。其余 `hardware_concurrency` 命中分布在 ACR（不链）、orchestrator（不在 `acsd` 闭包）、oracle、vendored `nanoflann.hpp`、`parser.cpp:256`（只拼指纹字符串 `astrocs-cpu-amd64-hw=N`，不用于 worker 数）——都不构成生产第二预算源。

**(f) 我另查到的一条真实"两处预算同时生效而互相看不见"路径（成稿未记，比 (e) 严重）**

- 合同自陈的唯一预算语义：`lib/include/astrocs/core/context.h:110-111`「acquire(min,max,policy) 原子预留 token；`sum(active)<=budget` 全局不超卖；**Scheduler 自身 worker 与节点内部 work 共用同一预算**」；`lib/include/astrocs/core/executor.h:8-10` 同义重申（"worker 领取任务前经 `ThreadBudget::acquire` 预留 ⇒ Σ(active) ≤ budget"）。
- 实测该不变量**只覆盖节点侧一半**：`lib/infrastructure/scheduler/src/scheduler.cpp` 全文对 `budget_obj_` 只做 `create_thread_budget`（`:28-30`）与 `ctx.set_budget(budget_obj_)`（`:98-100`），**从不调用 `acquire`**；`:373-375` 直接 `for (uint32_t i = 0; i < budget_; ++i) pool.emplace_back(worker);` 起 `budget_` 个常驻 worker。
- 于是 `available_` 起始即整份 `budget_`，节点内层池再各自 `acquire`（被 `dispatch_budget_hint` 夹到 `budget/同批在途数`，`context.cpp:226-238`）。全部 worker 都在跑节点的稳态下：**存活 OS 线程 ≈ budget（调度器常驻池）+ Σ 在途租约 ≤ 2×budget**。
- ⇒ 对题问"有没有一条路径能让两处预算同时生效而互相看不见"的**肯定答案**在这里，不需要 (e)。
- 附带术语校准：`upm.cpp:620`/`sampler.cpp:934` 等池是**函数作用域 + join**（`upm.cpp` 5 处 join、`scheduler.cpp:376` join），故 §9:673 字面"**不私建长期线程池**"的"长期"限定不满足；真正被违反的是 §8.3:607"**线程池的唯一来源是调度器**"（无"长期"限定）与 `scheduler.h:31`「禁止第二套全局调度器；模块只投递 work，不建私有 pool」。措辞若落在"违反不硬编码线程"上即判错。

**(g) 门本身可绕过（成稿未记）**：`eng/tools/arch/check_thread_budget.py` 是钉这件事的门，两处结构性洞——

1. **豁免是文件级**：`scan()`（`:283`）`is_exempt = any(_posix(k) in rel for k in EXEMPT)`，命中即整文件放行（`:296` 打印"文件级豁免"）。`REGISTERED` 是 **25 个文件键**、无 occurrence 计数 ⇒ 已登记文件内再加池不会变红（现实：`module_adapters.cpp` 已 2 个、`upm.cpp` 已 5 个）。
2. **扫描面按后缀漏 `.inc`**：`:276` `if not fn.endswith((".cpp",".c",".h",".hpp")): continue` ⇒ (c) 找到的生产池 `baseline_kernels_impl.inc:72` 对门**完全不可见**（它被 4 个生产 backend TU 编入）。门 docstring `:14-15` 自陈"closing: 全仓 **14** 处池声明此前对正则不可见"——成稿的"14"正是这个**门所及面**的计数，不是生产私建池的真值。

### 与成稿差异

- 成稿 AUD401-007/008 的事实面（executor 不在核心库源清单、靠 `:255` 文本包含带入、私建池多点、`execution_options.h` 有 `hardware_concurrency` 默认、"注入面干净而池面私建"）**全部独立复现**。
- **计数差**：成稿"生产 TU 内 14 处（executor 自身 3 处不计）"。我复算：生产编译真池声明 = 18；executor 真池 = **2**（成稿把 `executor.cpp:12` 的**注释行**算作 1 处）⇒ 私建 = **16**；成稿漏列 `lib/infrastructure/cli/commands.cpp:1260` 与 `baseline_kernels_impl.inc:72` 两处。分档实测：算法模块内 **6**（与成稿同）、基建内 **10**（成稿 7+1=8）。
- **风险归因差（实质分歧）**：成稿把"第二预算源"押在 `hardware_concurrency` 默认上；我实测它不可达于 `acsd`（(e)），押这条会**高估该理由并漏掉真问题** (f)。
- **新增**：(g) 门的两处可绕过 + "14"这个数其实是门自身口径的产物。
- 成稿 AUD401-007 标题"不进生产 target"与其正文（`:118`"寄生在 `module_adapters.cpp` 里"）用词不一致；须按 (b) 的四档分开表述。

### 定级建议

- **(f) 调度器常驻池不入账**：**P1**，建议与 (c) 同批立案但单列。触发路径确定（B 个 worker 全忙 + 各自节点起子池），后果是稳态 2×budget 线程，且 §8.3:611 静态预算/内存回压按 `budget` 校准却面对 2× 并发 ⇒ 回压判据失真。不到 P0：§8.3:616 的"数值与并发度无关"不变量兜住结果正确性，暂无出错证据。
- **(c)+(b) 私建池 ＋ 文本包含走私**：**P1**（架构治理面）。"唯一池"是 §8.3:607 正向约束，实现把它降级为"唯一数量源"，且防重定义只靠一行注释。
- **(g) 门可绕过**：**P1**。按 AGENTS §9「门本身不合理时改进门本身」，此条优先级不低于被它管的实体。
- **(e) `hardware_concurrency` 第二默认**：**P2**（硬化项，非生产路径）。成稿若把它按 P1 立案，我判**降级**。

### 缺什么证据

- (f) 的稳态倍数目前是静态推导，需要一次带探针的实测线程数曲线（`ProbeSink`/`NodeTrace` 已有节点 worker 观测面）在 4/8/16 worker 档核对；本层禁跑构建与测试，未取。**若实测确认 2×，(f) 应从 P1 升 P0**（内存回压失真属科学不变量级）。
- 无其它缺口。

<!-- PROGRESS: 3/4 -->
