# 审稿 P1 — INF-benchmark-001（G08-05 对抗审稿 第 1 遍）

- **片号**：`INF-benchmark-001`
- **层**：`lib/infrastructure/benchmark`（生产源码）
- **基线**：仓库 `/workspace/Astro CS Database`，**实测 HEAD = `1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`**（派单写 `850a9ede`，与实际不符；本报告一律以实际 HEAD 为准，全部结论在该树上复现）
- **口径**：行数 = `wc -l` 物理行；「读」= 用 read 工具逐行取得带行号原文，不接受 grep 摘要、机器词表或他人转述
- **纪律**：零 git 写；未编译、未跑 ctest/pytest/构建/任何仓内二进制；未修改任何仓内文件；未读 `/tmp/acsd_g08/`

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **45** |
| 实读份数 | **45** |
| 成员总行数 | **8389**（与清单「实际行数 8389」一致） |
| 实读行数 | **8389** |
| 覆盖率 | **100.0%** |

**未读完的：无。** 45 个成员文件全部由我用 read 工具逐行读完。

口径说明：清单 `成员份数: 45 / 实际行数: 8389` 与我实测完全一致（`git ls-files lib/infrastructure/benchmark | wc -l` = 45，磁盘 45 个文件，无未跟踪残留）。45 份中 4 份是 Markdown/JSON/schema，41 份是 C/C++ 源码与头。

---

## 2. 本片判定：**阻断**

判定理由（三条最重，全部是我自己读完原文后独立构造反例坐实的）：

### 阻断 1 — 生产路由的唯一判据是档案自己报的字段；两个消费入口一个弃权、一个弃约

`cpu_routing.cpp:515-523` 是生产实际的选路分支（`commands.cpp:327` 传 `live_rows=nullptr` ⇒ `trusted_profile_rows=true`）：

```cpp
if (trusted_profile_rows) {
    d.provider = want_provider;   // want_provider = jget_ref(*kp, "provider")  ← 档案自报字段
    d.stage = "select";
    d.ok = true;
```

到达这里之前只做了三件事：结构校验（`verify_profile_v2`）、身份校验（`check_profile_identity_v1`）、以及 `profile_kernel_benchmark_valid` 的「`correctness_test=="oracle:pass"` 且 median 有限正」。**没有任何一次重测，没有任何一次收益重判。** 即：档案说「用 avx2」，路由检查「档案说用 avx2 且 median>0」，于是用 avx2。契约写在 `cpu_routing.h:108-109`（「为 nullptr 时 benchmark 阶段只校验 profile 记录行…select 取 profile 行」），所以这是**已声明的设计**，我不按「违约」论；我按**它使全部上游缺陷无第二道门**论——而上游缺陷密度很高（见阻断 2 与第 4 节的自洽式断言清单）。

### 阻断 2 — 唯一被强制要求的 fail-closed 硬停，被报告层整条丢弃

`profile_gen.h:102-104` 对 `ProfileBundle::violations` 的合同是强制的：

> 非空 ⇒ 该 profile 结构性不可写盘：**调用方须 fail-closed**(CLI 返回 acsd::CRASH)，不得落盘再等复读层拒收。

`profile_gen_v2.cpp:723-727` 复述同一句。两个活调用者：

- `commands.cpp:2642-2648` **遵守**：非空 ⇒ 打印 + `return acsd::INTERNAL`。
- `bench_report.cpp:190-201` **不遵守**：拿回 `bundle` 后只用了 `raw` / `raw_samples_sha256` / `json`，`bundle.violations` **一次都没被读**，随后 `benchmark_report.cpp:200` 独立聚合、`:201` 出 verdict、`:324` 产出 `eligible_for_profile` 的报告全文。

后果：同一台机器、同一份 `generate_profile_v2` 结果，CLI 判 `INTERNAL` 拒写，报告层判 `eligible_for_profile=true` 并可落盘。`verify_benchmark_report`（`bench_report.cpp:328-421`）**不可能发现这个矛盾**，因为它从不接触 `violations`、也从不接触 `RawCandidate`。

### 阻断 3 — 宿主 worker 预算不变量 `Σactive ≤ max_workers` 有三个可触发破口，且声称覆盖它的测试从未进入该分支

三处 provider 家族的 `run_banded` 都是同一形态：**丢弃 `acquire(1)` 的返回值，却无条件 `release`**：

| 文件 | 丢弃 acquire 返回 | 无条件 release |
|---|---|---|
| `cpu/baseline/src/baseline_provider.cpp` | `:428-429` `(void)host->executor->acquire(user_data, 1)` | `:455-457` `release(user_data, workers)` |
| `cpu/avx2/src/avx2_provider.cpp` | `:219-220` 同上 | `:240-242` 同上 |
| `cpu/avx512/src/avx512_provider.cpp` | `:232-233` 同上 | `:253-255` 同上 |

宿主侧计数器 `host_services.cpp:23` 是 `std::atomic<long> active_workers`（**有符号**），`host_acquire:75` 的门是 `if (cur + n > cap) return ACS_ERR_BUDGET`，而 `host_release:83` 是**无下溢保护的 `fetch_sub`**。

于是构造：`max_workers=4` 被别处占满 → `acquire(1)` 返回非 0 → 代码仍跑 kernel → 退出时 `release(1)` 而**从未 acquire 成功** → `active_workers` 变 −1。此后 `cur + n > cap` 恒更容易满足 ⇒ **预算门永久失效**，后续可租借远超 `max_workers` 的 worker。这是「伪装成 fail-closed 实则 fail-open」的教科书形态。

legacy 家族的对应点是 `baseline_kernels_impl.inc:62-64`：`acquire(1)` 失败时**无租借照跑**，且无 logger、无错误码；**同库内** `backend_table.inc:55` 对完全相同的失败条件返回 `ACS_ERR_BUDGET`。同库失败语义不一致。

**为什么这条升级到阻断**：`eng/tests/unit/cpu_backend_exception_test.cpp:62-67` 的 `exhaust_acquire` 注释声称覆盖「预算耗尽仍须可跑(串行)」这条分支，但 `if (n > 1) return 1;` 意味着 **`acquire(1)` 恒返回 0** ⇒ `:59-61` 有租借分支永远命中、`:62-64` 无租借分支**从未被执行**。测试断言（`:142` `CHECK(g_acquired == g_released)`）在两条分支下都成立，也无法区分。**判据不可信，且已被实测证伪。**

---

## 3. 逐文件清单（45 份，每份：读了什么 → 看到什么 → 判定）

### backend_host（30 份）

| 文件 | 行 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| `profile_gen_v2.cpp` | 966 | 全文件：kSpecs/oracle_ref/build_inputs/fill_params/R-53 三函数/generate_profile_v2 全流程/verify_profile_v2 | `oracle_ref` 是**真正独立**的 double 重实现（判定正面）；但 `:563 (void)blk` block 维空转、`:568 c.workers=w_cand` 丢弃 `br.workers`、`:674 sp.workload[0]!='\0'` 恒真、`:684 break` 在 if 体内、`:446-450` 吞 manifest 失败、`:511-512` 用 cli_sha256 冒充自检 hash、`:960-962` 只验 profile_id 形态 | **须修（多处）** |
| `profile_gen.h` | 145 | 全文件 | `RawCandidate:28-32` **有默认成员初始化**（否掉了我对未初始化字段的怀疑）；`:90` 声明 `self_test_sha256` 为「64hex; 空=未运行」，实现塞三种语义；`:102-104` violations 强制条款被下游违反 | 须修（声明侧） |
| `profile_gen.cpp` | 152 | 全文件（v1 旧 schema） | `:48` 忽略 `acsd_backend_get_api_v1` 返回值 ⇒ `:126 all_pass` 保持 true ⇒ `:147 verdict="PASS"` + **空 kernels 数组**；`:117 p.op=CALIBRATION` 硬编码而 `:125-129` 遍历 12 个 entry；`:46` 预算硬编码 `2,2`；`:33` 全局 `lcg_state` | **须修（恒绿门）** |
| `profile_store.cpp` | 425 | 全文件 | `:266` 已保证逐字节相同，`:271` 再喂同一串给纯函数 `verify_profile_v2` ⇒ **恒真门，永不可达**；`:206-216/:327-336/:353-362` 把档案**自身**的 `source_commit` 当期望值；`:333` 窄 catch 漏 `out_of_range`；`:314/323/345/374` 丢弃 `atomic_replace` 返回值仍填 `rejected_path`；`:242-251` 孤儿清理不区分 pid/年龄 | **须修（自洽式断言）** |
| `profile_store.h` | 117 | 全文件 | `:20-27` 原子写协议段落与实现逐条对应且正确（**本片质量最高的一段**）；`:26` 说「下次写前被清理」但实现不区分活进程；`:85` 说空串=「跳过校验」，实现是「自比较」 | 须修（声明不符） |
| `bench_harness.cpp` | 240 | 全文件 | `:54` 是本片**唯一真正独立**的判据（被检量=kernel 实跑输出，期望量=调用方独立 oracle，否定正面）；但 `:170 (acc==1234.5678?1.0:0.0)` **可证恒假**（acc 是整数和，1234.5678 非整数），自称的防 DCE 守卫不存在；`:51 n=p.w*p.h` 与 `out.size()`/`expected_ref.size()` 三者互不校验；`:86 mad_ns` 实为平均绝对偏差却叫 MAD；`:68/:75` 忽略 `fn` 返回值 | **须修** |
| `bench_harness.h` | 77 | 全文件 | `:28` 声明 `samples≥7`、`:27` 声明 `expected_ref` 独立，**实现侧均无强制**；`:71 NoProfilePolicy::reason` 是无默认初始化的裸指针 | 须修 |
| `bench_report.cpp` | 423 | 全文件 | `:137-148` 链式 tie-break **可选出比集合最小值更慢的胜者**（已构造）；`:73-75 is_heavy` 恒真；`:202/:315/:413` `production_profile_touched` 字面量对字面量；`:229/:356` `outlier_policy_frozen` 同款；`:190-201` 丢弃 `violations`；`:346-349` `report_id` 与 `input_samples_sha256` 同源却从不互校 | **须修（自洽式断言）** |
| `bench_report.h` | 116 | 全文件 | `:74` 明确契约是 `best = min(median_ns)`，被 `:137-148` 违反；`:40-41` 宣称 kBenchWarmup/Samples 是「唯一出处」，真执行处 `profile_gen_v2.cpp:565` 是字面量 `3,7`；`:8` 「命令有 timeout」在库层只是事后标注 | 须修 |
| `worker_advisor.cpp` | 315 | 全文件 | `:222 a.workers = 2` 与同文件头 `worker_advisor.h:7-9`「no fixed cores — 无任何固定核数(2/16/32 等)分支」**直接抵触**；`:242 worker1_allowed` 可证恒真 ⇒ `:243` 死分支；`:72-73 l2_bytes_from_hw` **空 catch 体**；`:105-155 derive_limits_v1` 从不产出 `ram_headroom_bytes`/`per_worker_mem_bytes` ⇒ `:93-97 mem_cap_v1` 恒返 0 ⇒ `:175-177` 内存钳制结构性死掉；`:186-187` 与 `:229-231` 在 io 类无 profile 时写出自相矛盾的 chain | **须修** |
| `worker_advisor.h` | 127 | 全文件 | `:74-75` 声称 workload 词表「与 profile v2 workload_class 对齐」含 `tiny`/`io`，而 `profile_gen_v2.cpp:69-80` 的 kSpecs **只产出 compute/memory** ⇒ 死分支；`:7-9` 与实现 `:222` 自相矛盾 | 须修 |
| `baseline_kernels_impl.inc` | 314 | 全文件 | `:62-64` 预算耗尽无租借照跑（无 logger、无错误码，与 `backend_table.inc:55` 同库不一致）；`:264/:281-282` 栈类 op 入参只比 `.count` **不比 `.data != nullptr`**；`:251 P.w*P.h` 32 位乘法只挡 `N==0`；`:308-310 const_cast` 穿 `const void*` 写调用方内存；`:52 while(workers<cap) workers<<=1` 在 `cap>2^31` 时溢出为 0 ⇒ 死循环 | **须修** |
| `baseline_kernels.h` | 51 | 全文件 | `:43 workers_used` 是 out 字段，与 `common_abi_v1.h:135` 的 `const void* params` 签名冲突（两份合同不可能同时成立）；`:3` 引 `ARCH-004 §4` 仓内无定义 | 须修 |
| `cpu_routing.cpp` | 612 | 全文件 | `:515-523` 生产选路取档案自报字段（阻断 1）；`:137` `prof_q.empty()` ⇒ 拒；`:141` 默认 0 与生成侧 `profile_gen_v2.cpp:423` 默认 1 不一致；`:361 !(gain>min)` **fail-closed（正面）**，与 `profile_gen_v2.cpp:666 gain<0.03` **NaN 语义相反**；`:600-601` 12/12 全部 fallback 时仍报 `"decision":"all_baseline"` | **须修** |
| `cpu_routing.h` | 156 | 全文件 | `:2/:71/:108` 三处引 `15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md`（**已删**）；`:108-113` 如实声明了「不重测」契约（诚实，但使其成为无第二道门的权威） | 须修（悬空） |
| `hardware_inspect.cpp` | 363 | 全文件 | `:300 architecture="amd64"` 字面量；`:58 cpuinfo_field` 前缀是**子串匹配**、`:63` 的空行 break 因 `getline` 已剥换行而**永不触发** ⇒ 扫全文件取首个匹配；`:203-205` `sscanf` 失败 ⇒ `family/model/stepping` 静默 0 ⇒ **两台不同机器都退化成 0 且比较相等**；`:279-281` `bool ok=false` 后 `(void)ok` 死变量；`:230-234` cache 循环遇空即 break ⇒ cpu0 离线的容器拿空数组且无诊断；`:253-256` `smt.enabled` 用 L1 `shared_cpu_list` 是否含逗号判定 ⇒ **Intel 上恒错**；`:359 backend_hashes` 恒空数组却注释称「与 backends.manifest.json 联动」 | **须修** |
| `hardware_inspect.h` | 19 | 全文件 | `:2` 声称输出与 `cpu_profile.schema.json` 的 hardware 对象「同构」，实测键名全不同（`architecture` vs `arch`、`feature_names` vs `features`、`available_logical_cpus` vs `logical_available`、`os.name` vs `os_abi`） | 须修（声明不符） |
| `backend_loader.cpp` | 203 | 全文件 | `:184 if (out_api->self_test && ...)` ⇒ **self_test 指针为空则整个自检被跳过并返回 OK**；`:169-187` 把 `REJECT_SECURITY` 与良性回落**同码处理**，安全事件被洗成 fallback_reason；`:62-67` 精确错误信息交给调用方，而 `profile_gen_v2.cpp:450` 把它丢掉；`:27-35 is_bare_filename` 不拒内嵌 NUL ⇒ `path.c_str()` 截断后加载的是另一个文件 | **须修（静默降级）** |
| `backend_loader.h` | 55 | 全文件 | `:42` 诚实声明 `threadsafe=no(句柄级)`（正面）；`:26-30` 契约明确「结构非法→err 非空, 不猜」 | 通过（声明侧） |
| `cpu_features.cpp` | 132 | 全文件 | `:45-49` AVX-512 **五个子集逐位置位**、`:58-59` XCR0 0x6/0xE0 双门（**正面，正是本仓反复点名的正确形态**）；`:52` 的 avx512_group 额外并入 CD，而 `cpu_features.h:30-31` 的 PROVIDER_REQUIRED **不含 CD** ⇒ 两个 ISA 判据不等价；`:27-33` `static bool have_xsave` 非原子；`:109` 非 x86 返回 0 | 须修（跨文件不一致） |
| `cpu_features.h` | 43 | 全文件 | `:30-31` AVX512_PROVIDER_REQUIRED 只有 F|BW|DQ|VL（**四**位），与 `avx512_provider_v1.h:88-89` 的**五**位、capability_detect 的**五**位不一致；本头 10 位 vs `capability_v1.h:49-63` 13 位 vs `hardware_inspect.cpp:220-222` 只映 6 位 ⇒ **同一「特性」概念三套互不覆盖的词表** | **须修** |
| `host_services.cpp` | 152 | 全文件 | `:83 host_release` 无下溢保护的 `fetch_sub`（阻断 3 的宿主侧）；`:74-79 CAS 用 relaxed **对纯计数器是正确的**（否定该怀疑）；`:36` 对齐取整在 `size` 接近 SIZE_MAX 时溢出；`:96 new HostState` 可抛 `bad_alloc` 穿过 `extern "C"`；`:144-150 set_budget` 不 clamp `max_workers`、不查空指针 | 须修 |
| `backend_table.inc` | 121 | 全文件 | `:21,22,26,27,28,31` 六个 kernel 声明 `ACS_PRECISION_F64`，而 `baseline_kernels_impl.inc` 全 float —— **同库 `profile_gen.cpp:134` 对同一列表写 `"precision":"fp32"` 自证矛盾**；`:90-91` API 级 `precision_class=F32/determinism_class=FIXED_ORDER` 与条目级 6×F64/6×BITWISE 互相打架；`:88 detected_features` 恒 0 且无人读；`:81` lazy-init 在 `threadsafe=yes` 声明下有数据竞争；`:36-61 backend_self_test` **不做任何 kernel 冒烟**（与 `baseline_provider.cpp:18` 注释「kernel 表自洽 + 冒烟」不符）；`:112/:239` 用 `!=` 严格等长 ABI 校验，与 `baseline_provider.cpp:465` 的 `< sizeof` 策略相反 | **须修** |
| `baseline_backend.cpp` | 55 | 全文件 | `:3` 头注释「kernel fn 科学实现属 ABI-003(**当前返回 ACS_ERR_UNSUPPORTED**)」与同 TU 内 `.inc:235-312` 的 12 op 完整实现**直接矛盾**；`:41-53 acsd_abi_boundary_probe` 在**同一栈帧内抛并捕获** ⇒ 它证明不了它命名的「异常不跨 kernel ABI 边界」，真机制是 `run_body_guarded` | **须修（自洽式断言）** |
| `avx_backend.cpp` | 42 | 全文件 | 退役登记注释**逐条经得起核实**（未被任何 target 引用；未定义 `ACSD_BACKEND_REQUIRED_FEATURES` ⇒ 回落 0 会致预检恒真）；`:19` 明写「= AGENTS §6 的统一注释块」 | **通过（本片最诚实的文件之一）**；仅 README 未标其退役 |
| `avx2_backend.cpp` | 37 | 全文件 | 门面 TU 零 ISA 旗标 + `#define kernel_dispatch` 宏别名指向计算面 ⇒ 变体**不会退化成同码**（设计正确，负面）；`:17` 显式声明 `AVX2\|FMA` | 通过 |
| `avx512_backend.cpp` | 52 | 全文件 | `:27-32` 按 `__AVX512CD__` 取工具链精确许可面，声明/编译/检测三侧同源（**本片最好的设计**）；`:26` 引 `eng/tools/quality/check_variant_isa_disasm.py` —— **实测 MISSING** | 须修（悬空引用） |
| `avx2_backend_kernels.cpp` | 50 | 全文件 | `:24-26 #error` 把「旗标被忽略」变成**编译期红灯**（正是判据可证伪的正确形态）；`:41` 与 baseline 共用同一 `.inc` ⇒ 零复制漂移为真 | 通过 |
| `avx512_backend_kernels.cpp` | 43 | 全文件 | 同上；`:17-19 #error` | 通过 |
| `backend_variant_kernels.h` | 36 | 全文件 | `:28 acsd_variant_kernel_dispatch_v1` **无 hidden visibility**，而同目录第二家族的 `cpuprov_kernels_v1.h:44` 有 ⇒ **两个家族的 DSO 导出面不对称**，与 `:26`「ABI 不变」的说法不符 | 须修 |
| `README.md` | 19 | 全文件 | 组件描述逐条属实（核过）；`:13` 把已退役 `avx` 与在产变体并列未标注 | 须修（轻微） |
| `PENDING.md` | 13 | 全文件 | `:8` 总数 38 → **实测 45**；`:9` backend_host 27 → **实测 30**；`:10` cpu 10 → **实测 13**；`:12` 「缺 README.md」→ **README.md 存在且已跟踪**、`:12` 称 MODULE_MAP 声明了 `readme:` 字段 → **该字段全文件 0 命中**；`:5-6` 治理规则指向 `eng/tools/quality/check_module_map.py` → **实测 MISSING**；`:8` 还带「2026-09-20 实测」日期（AGENTS §5 禁） | **须修（登记面三个计数 + 两项存在性断言全错，且自称「如实登记」）** |

### cpu/（13 份）

| 文件 | 行 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| `cpu/baseline/src/baseline_provider.cpp` | 619 | 全文件 | `:45/:448-453` 按头注释定义的定义实现「仅有限帧参与 median/MAD」，而 **benchmark 实际执行的 legacy `.inc:121-122` 与 `oracle_ref` 都没有该过滤** ⇒ 两实现语义分叉；`:52-55` `acsd_cpu_baseline_cap_gate` 是 extern "C" 顶层符号而 `:4-5` 声称「唯一导出」；`:420` workers 翻倍循环在 `cap>2^31` 溢出 ⇒ `N/0`；`:429+457` 阻断 3；`:408-459` 每次调用 `std::thread` 现场起线程，且 `emplace_back` 可抛而 `baseline_run_kernel` **无 try/catch**（与 `:25-26` 「导出 C 函数内捕获」矛盾）；`:549-565` 校验开销 O(N)+O(nnz) **被计入被测时间**；`:95/:97` drizzle-overlap / drizzle-normalize 是逐元素无序，却被标 `DET_FIXED_ORDER` | **须修（多处）** |
| `cpu/avx2/src/avx2_provider.cpp` | 368 | 全文件 | `:220+242` 阻断 3 同款；`:8-10` 把 +20.7%/+11.7%/+28.2%/+28.3% 的**唯一出处**指向 `实验/engineering-evidence/prerelease-v5/`；`:191` 「cap_gate 函数体含 18 处 VEX/EVEX」是无命令的硬编码计数；`:143/:309` N 用 64 位且拒 `N>UINT32_MAX`（**比 legacy 的 32 位好，负面**） | 须修 |
| `cpu/avx512/src/avx512_provider.cpp` | 382 | 全文件 | `:82-87 ACS_CPU_AVX512_CAP_GATE_NOEVEX` 在非 GCC/Clang 展开为空——但因 R-60 已把计算面移到零旗标门面 TU，MSVC 侧 cap_gate 仍安全 ⇒ **该怀疑被推翻**；`:80` 引的 `eng/tests/cpu/avx512/check_avx512_illegal_instr.py` **实测存在**（否掉子代理 E 的推广）；`:233+255` 阻断 3 同款；`:205` 同款硬编码计数 | 须修 |
| `cpu/common/src/capability_detect.c` | 387 | 全文件 | `:191 memcpy(r.vendor, v, 16)` 从 `char v[16]` 拷 16 字节，**v[13..15] 是未初始化栈**进结构体（被 `:351` 的 `%s` 在 `v[12]='\0'` 处挡住，故不进 JSON，**但进了结构体字段**）；`:220-239` brand 累积有 `off<sizeof(b)` 全程守卫 ⇒ **不溢出，但 48 字符 brand 末位必被 `b[47]='\0'` 截掉**；`:142-150` AVX-512 五子集组包含（**正面**）；`:350 architecture` 字面量；`:284` `a.truncated` 写入但从不读 | 须修 |
| `cpu/common/include/acsd/cpu/capability_v1.h` | 159 | 全文件 | `:116-118` 承诺 ABI 失配时「**先写 `*out->struct_size`**」，而 `capability_detect.c:173-176` 是 `*out = r` **整结构覆写** ⇒ 失败语义文档被违反；`:21/:68/:129` 三处引已删的 `15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md`；`:41 BRAND_MAX=64` 与探测实际产 48 字符不匹配 | 须修 |
| `cpu/common/include/acsd/cpu/cpuprov_kernels_v1.h` | 61 | 全文件 | `:44` hidden visibility（**正面**）；`:9/:24-28` 把 `run/FINAL-07/审核包/工程/变体能力面一致性报告.md` 当设计依据引用 —— 治理运行产物不是规范正本 | 须修（层级错误引用） |
| `cpu/common/include/acsd/cpu/baseline_provider_v1.h` | 149 | 全文件 | `:136-143` `sizeof==136` 布局静态断言（**优秀反漂移，负面**）；`:12` 引的 `docs/engineering/CPU_BACKEND_ARCH.md` **实测存在**；`:50-52` 冻结的 NaN 语义条款**被 legacy 实现违反**（见上）；`:84` 硬编码 `"CPU-002-0d32c07"` 永不会跟真实 commit | 须修（声明 vs 实现） |
| `cpu/avx2/include/acsd/cpu/avx2_provider_v1.h` | 108 | 全文件 | `:100-102` 变体 params POD 与 baseline 静态断言相等（**优秀，负面**）；`:79-86` 的热点/不热点清单全部指向 `实验/engineering-evidence/prerelease-v5/`；`:11` 引 `02_CURRENT_BASELINE_AUDIT` | 须修（悬空/退役证据） |
| `cpu/avx512/include/acsd/cpu/avx512_provider_v1.h` | 115 | 全文件 | `:88-89 ACS_CPU_AVX512_REQUIRED_FEATURES = ACS_CAP_GROUP_AVX512_SUBSET`（**五**位），与 legacy 的**四**位不等价；`:16-21` 收益台账指向同一已判退役的实验归档；`:107-109` 静态断言（负面） | 须修 |
| `cpu/avx2/src/avx2_kernels.cpp` | 90 | 全文件 | `:21-23 #error` 编译期红灯；`:55-56/:82-83` 与 baseline provider / `.inc` **逐字同式同项序**（我逐行比对确认，**负面**）；`:42-44` 明写「逐字符搬移」 | 通过 |
| `cpu/avx512/src/avx512_kernels.cpp` | 84 | 全文件 | `:26-29` 第二道 `#error` 专门挡「声明 ⊋ 编译」（**优秀**）；`:76-77` 与上同式 | 通过 |
| `cpu/common/schemas/cpu_capability.schema.json` | 86 | 全文件 | `:63/:68` 的 13 个 feature 名与 `capability_detect.c:70-85 cap_bit_name` **逐条对齐**（否掉该项怀疑，负面）；`:27 architecture const amd64` 是恒真约束；`:23 additionalProperties:false` | 须修（arch 面） |
| `cpu/common/README.md` | 111 | 全文件 | `:3` 开篇元信息块 + 「状态: FROZEN」**在登记面自证状态**，而同目录 `PENDING.md:5` 明写「禁止在登记面自证」—— 同树两文件互相打架；`:13` 头路径 `lib/include/acsd/cpu/capability_v1.h` **不存在**，而 `:56` 给的是对的 ⇒ 同文件自相矛盾；`:4/:36/:59/:89` 引两份已删文档，`:25`「C §C3」与 `:59`「C §C6」两个节号指同一约束；`:13/:110` 已过期（CPU-002/003 已交付）；`:107` 无命令无产物路径的实测声明；111 行模块规格占据 AGENTS §5 要求「极简 README」的位置 | **须修（形态 + 悬空 + 自证）** |

---

## 4. 发现清单

### 【阻断】5 条

- **BL-1** 生产选路只认档案自报字段，无重测无收益重判 —— `cpu_routing.cpp:515-523`（契约见 `cpu_routing.h:108-113`，故非违约，而是**使全部上游缺陷失去第二道门**）
- **BL-2** `ProfileBundle::violations` 的强制 fail-closed 条款被报告层整条丢弃 —— 合同 `profile_gen.h:102-104` / `profile_gen_v2.cpp:723-727`；遵守方 `commands.cpp:2642`；**违反方 `bench_report.cpp:190-201`**
- **BL-3** 三 provider + legacy 的 `run_banded`：丢弃 `acquire(1)` 返回值却无条件 `release` ⇒ 宿主 `active_workers` 可变负 ⇒ `Σactive ≤ max_workers` 永久失效 —— `baseline_provider.cpp:429/455-457`、`avx2_provider.cpp:220/240-242`、`avx512_provider.cpp:233/253-255`、`host_services.cpp:83`（无下溢保护）、`host_services.cpp:75`（门依赖该值）；legacy 侧 `baseline_kernels_impl.inc:62-64` 无租借照跑且与 `backend_table.inc:55` 同库语义不一致
- **BL-4** `baseline_kernels_impl.inc:264/281-282` 栈类 op 入参只校验 `.count` 不校验 `.data != nullptr` ⇒ 越过校验后在 `:122/:167/:200` 解引用空指针；同文件其余所有 span 都走 `spans_ok`（`:94-96`）
- **BL-5** `PENDING.md:8-12` 三个计数（38→45、27→30、10→13）与两项存在性断言（README.md 存在、`readme:` 字段全文件 0 命中）**全错**，且文件自称「如实登记，2026-09-20 实测」；`:5-6` 的治理规则指向实测 MISSING 的 `eng/tools/quality/check_module_map.py`

### 【须修】22 条（按主题归并，均带 文件:行）

**A. 自洽式断言 / 恒真门（本轮最高价值，均为我独立构造）**
1. `profile_store.cpp:266` 已保证 tmp 与内存串**逐字节相同**，`:271` 把同一串再喂纯函数 `verify_profile_v2` ⇒ **恒真门，永不可达**；`:262` 注释「防内存对、盘上错」的真实保障只有 `:266` 那一行
2. `profile_store.cpp:206-216` / `:327-336` / `:353-362`：`current_commit` 为空时用**被校验档案自身的** `build.source_commit` 当期望值 ⇒ commit 绑定自比较。而 `cpu_routing.cpp:237` 明确把空 commit 定义为**必须失败**（fail-closed）—— 存储层把这个 fail-closed 改写成了自比较
3. `bench_report.cpp:202` 与 `:315` 两次写字面量 `false`，`:413` 再把 JSON 里的 `false` 判为合法 ⇒ 同一常量自比
4. `bench_report.cpp:229` 写 `outlier_policy_frozen:true`，`:356-357` 校验 `== true` ⇒ 没有任何东西能令其变 false
5. `bench_report.cpp:375-404` 的复读循环从不接触 `RawCandidate`；`:416-419` 用报告自报的三个布尔互比 ⇒ 整个 `verify_benchmark_report` 无法发现任何聚合算错
6. `baseline_backend.cpp:41-53` `acsd_abi_boundary_probe` 同帧抛并捕获 ⇒ 证明不了它命名的跨 ABI 属性
7. `architecture` 在 6 处全是 `"amd64"` 字面量（`hardware_inspect.cpp:300`、`capability_detect.c:350`、`schema:27`、`profile_gen_v2.cpp:744`、`verify_profile_v2:867`、`cpu_routing.cpp:125`）⇒ arch 身份面完全空转；v1 的 `profile_gen.cpp:81` 曾真读硬件，v2 换成了常量
8. `bench_harness.cpp:170` `(acc == 1234.5678 ? 1.0 : 0.0)` **可证恒假**（`acc` 是 `a[i]=(float)(i%1024)` 的整数和，1234.5678 非整数）⇒ 注释宣称的防 DCE 守卫不存在
9. `worker_advisor.cpp:242 worker1_allowed` **恒真**（穷举：`have_row` 路径、fallback 路径、clamp 路径三条都推不出 false）⇒ `:243` 死分支、plan 字段无信息量
10. `bench_report.cpp:73-75 is_heavy` **恒真**（词表只有 compute/memory）⇒ `bench_report.h:17-18` 的「重/轻区分」是装饰
11. `profile_gen_v2.cpp:674 sp.workload[0] != '\0'` **恒真**（kSpecs 12 条 workload 全非空）
12. `profile_gen_v2.cpp:334 profile_invariant_violation` 的 `mad_ns < 0` 分支**不可达**（`median_inplace` 恒 ≥0、steady_clock 恒 ≥0）⇒ 对应 `bench_report.cpp:384` 的 mad 门是恒真门
13. `profile_gen_v2.cpp:502/504` 的 `p.api.self_test &&` ⇒ **自检指针为空时自检被跳过**，provider 仍进 `usable_providers`；`backend_loader.cpp:184` 同款 ⇒ 两处独立
14. `profile_gen_v2.cpp:511-512 / 708-709`：`self_test_sha256` 在 provider 无 hash 时回落 **CLI 主二进制 hash**，与 `profile_gen.h:90` 声明的「provider self_test hash」语义不符；`:506` 还塞入 13 字符哨兵 `"selftest_fail"`

**B. 筛掉真信号**
15. `bench_report.cpp:137-148`：链式 tie-break 的 `rel` 相对**移动中的 `best`** 计算，早期候选永不回访 ⇒ 可选出比集合最小值更慢的胜者（构造见 §5-D1）
16. `bench_report.cpp:126-135`：heavy 过滤器先把 `workers==1` 整类筛掉再取极值，被筛者的 `dispersion` **永不被检查** ⇒ 最可疑的样本消失后其余绿
17. `profile_gen_v2.cpp:675-686`：`:684 break;` 位于 `if (gain>=0.03)` **并列层**（不在其内），循环无条件在**第一个** workers≥2 候选处终止 ⇒ 中间档位（构造见 §5-D2）
18. `profile_gen_v2.cpp:563 (void)blk`：4 个 block 候选产生**完全相同**的执行，选择却在它们之间取 min ⇒ 上报 median 是 4×7 样本的最小中位数，系统性偏低；`executed_candidates`/`culled_candidates` 也被这个无区分维放大 4 倍

**C. 静默降级**
19. `profile_gen_v2.cpp:446-450`：manifest 读失败无 else、解析失败把 `merr`（`:449`）**写入后从不读取** ⇒ 整段 provider 装载被无声跳过；`backend_loader.cpp:62-67/86-87` 明明产出了精确原因。触发点在片外：`commands.cpp:2639` 传 `install_dir` 而非 `<prefix>/providers`
20. `hardware_inspect.cpp:53/58/63/203-205`：`/proc/cpuinfo` 读失败或 `sscanf` 失败 ⇒ `family/model/stepping` 静默 0，且 `cpuinfo_field` 的空行 break 是死代码 ⇒ 两台不同机器都退化成同一组 0，`check_profile_identity_v1:258-269` 比较后**相等放行**
21. `hardware_inspect.cpp:230-234`：cache 枚举遇空即 break ⇒ cpu0 离线的容器拿空数组；下游 `profile_gen_v2.cpp:481` 静默沿用 256KB 默认、`worker_advisor.cpp:248` 静默退到 `block_source="deferred"` ⇒ **三级静默**
22. `worker_advisor.cpp:105-155`：`derive_limits_v1` 从不写 `ram_headroom_bytes`/`per_worker_mem_bytes` ⇒ `:93-97 mem_cap_v1` 恒返 0 ⇒ `:175-177` 的「低资源不超配」规格结构性死掉

**D. 其它须修（本片内已定位，篇幅所限仅列标题与锚点）**
23. `backend_table.inc:21,22,26,27,28,31` 六处 `ACS_PRECISION_F64` vs 全 float 实现；同库 `profile_gen.cpp:134` 自证 `"fp32"`
24. `baseline_provider.cpp:311-323` NaN 语义（仅有限帧）与 legacy `.inc:121-122` + `oracle_ref` **无过滤**分叉，且 benchmark 测的是后者
25. `baseline_backend.cpp:3` 头注释与 12 op 完整实现直接矛盾
26. `cpu_features.h:30-31`（4 子集）vs `avx512_provider_v1.h:88-89`（5 子集）vs `capability_detect.c:146-148`（5 子集）—— 同一 provider 的 ISA 判据两套
27. 三套互不覆盖的 feature 词表：`cpu_features.h`（10 位）/ `capability_v1.h`（13 位）/ `hardware_inspect.cpp:220-222`（只映 6 位）
28. `worker_advisor.cpp:222 a.workers = 2` 与 `worker_advisor.h:7-9`「no fixed cores — 无任何固定核数(2/16/32 等)分支」**同文件族内直接抵触**
29. `bench_harness.cpp:51-53`：`n=p.w*p.h` 与 `out.size()`、`expected_ref.size()` 三者互不校验（后者偏小即越界读）
30. `bench_harness.cpp:68/75`：warmup 与计时循环**忽略 `fn` 返回值** ⇒ 中途失败的 kernel 仍贡献极小样本时间并可能胜出
31. `bench_harness.cpp:84-86`：`mad_ns` 实为**平均**绝对偏差，字段名与 `bench_report.h:39`/`bench_harness.h:21` 均称 MAD
32. `profile_store.cpp:333` 窄 catch（`parse_error`）漏 `:332 d.at("build")` 的 `out_of_range`；同函数 `:212/:360` 用的是 `catch(...)` ⇒ 同族三处口径不一
33. `profile_store.cpp:314/323/345/374`：`(void)atomic_replace` 丢返回值却**先**填 `rejected_path` ⇒ 改名失败时报假隔离
34. `profile_store.cpp:242-251`：孤儿 tmp 清理不区分 pid/年龄 ⇒ 并发两个 `acsd benchmark` 互删对方在途临时文件
35. 悬空引用四处：`PENDING.md:5-6`（check_module_map.py，MISSING）、`avx512_backend.cpp:26`（check_variant_isa_disasm.py，MISSING）、`capability_v1.h:21/68/129` + `cpu_routing.h:2/71/108` + `cpu/common/README.md:4/36/59`（`15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md`，已删）、`cpuprov_kernels_v1.h:9/24-28` + `avx2_backend_kernels.cpp:13`（治理运行产物当规范依据）
36. `baseline_kernels_impl.inc:251` 32 位乘法 + `:52` workers 翻倍在 `cap>2^31` 时溢出为 0 ⇒ 死循环 / `N/0`
37. `backend_variant_kernels.h:28` 无 hidden visibility，与 `cpuprov_kernels_v1.h:44` 不对称 ⇒ 两个家族 DSO 导出面不一致

### 【建议】9 条

`profile_gen.cpp:48+126+147` 恒绿门（空 kernels 也判 PASS）｜`profile_gen_v2.cpp:222` 与 `profile_gen.cpp:33` 的文件级可变 `lcg_state` 使「可复现」只在进程首次成立 ｜ `hardware_inspect.cpp:253-256` 用 L1 `shared_cpu_list` 含逗号判 SMT ⇒ **Intel 上恒错**，`bench_report.cpp:213-214` 的物理核估计随之偏大 ｜ `hardware_inspect.cpp:279-281` `bool ok` 死变量 ｜ `capability_detect.c:191` vendor 数组含未初始化栈字节 ｜ `baseline_kernels.h:43` 的 out 字段与 `const void*` 签名冲突（需 ABI 变更单）｜ `README.md:13` 未标 `avx` 退役 ｜ `cpu/common/README.md` 形态违规（111 行规格占 README 位 + `:3` 登记面自证状态）｜ `hardware_inspect.h:2`「同构」声明与实测键名全不符

### 已实测为零生产消费者的退役/死代码（grep 全仓 `lib` + `eng`，逐个符号核实）

| 符号 | 唯一消费者 | 性质 |
|---|---|---|
| `advise_kernel_v1` / `build_resource_plan_v1` / `worker_grant_trace_v1` / `derive_limits_v1` | `eng/tests/unit/cpu008_worker_advisor_test.cpp` | CPU-008 两个产物 schema 无任何写入点 |
| `generate_benchmark_report` / `verify_benchmark_report` / `aggregate_benchmark_kernels` | `eng/tests/unit/cpu006_bench_report_test.cpp` | CPU-006 报告层不在生产路径 |
| `load_profile_checked_v1` | `eng/tests/unit/cpu007_profile_store_test.cpp` | CPU-007 装载链在产品路径无实现 |
| `generate_profile_json`（v1） | `cpu_profile_test.cpp` / `profile_gen_main.cpp` | 仍编入产品静态库（`CMakeLists.txt:788`），full 模式结构性坏 |

**注意**：`avx_backend.cpp` 虽已退役，但被 `eng/tests/backend/test_isa_avx.py:20` 编译为独立 DSO 做 ISA 门 —— 它的「退役登记」应补这一句，否则按其 `:20` 的删除建议会连带删掉门禁输入。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| **D1** | `aggregate_benchmark_kernels` 输入 `eligible = [A(100.0, baseline), B(99.5, avx2), C(100.4, baseline)]` | `bench_report.h:74` 声明的 `best = min(median_ns)` | **推翻成立**。逐轮推演：best=A(100.0) → c=B 99.5<100 ⇒ best=B(99.5) → c=C 100.4≮99.5，`rel=|100.4-99.5|/99.5=0.0090<0.01` 且 `rank(baseline)=0<1` ⇒ **best=C(100.4)**。上报 median 比集合最小值慢 0.9%，且结果依赖 `cands` 推进顺序 |
| **D2** | `avail=8`；某 provider 实测 median `{w1:110, w2:100, w4:40}`，w8 候选 oracle 失败（不在 pass 集） | `profile_gen_v2.cpp:673` 注释的「worker 增加收益<3% 可少选」语义 | **推翻成立**。`:675` 循环在第一个 workers≥2 候选（w2）处因 `:684 break` 终止，`gain=(110-100)/110=0.0909 ≥0.03` ⇒ 采纳 w2/median=100；**收益 64% 的 w4=40 被完全屏蔽**；`:687-698` 兜底要求 `c.workers==avail==8` 且 `median>0`，而 w8 候选不存在 ⇒ 兜底不触发 |
| **D3** | 宿主 `max_workers=4` 已被占满 → `acquire(1)` 返回非 0 | 「租借失败必有稳定错误码 + 预算不变量守恒」 | **推翻成立**。`baseline_provider.cpp:428-429` 丢弃返回值继续跑，`:455-457` 无条件 `release(1)` ⇒ `active_workers` = −1；此后 `host_services.cpp:75` 的 `cur+n>cap` 门失效，可租借远超 cap 的 worker |
| **D4** | 让落盘层把内存串改写一字节（模拟磁盘损坏） | 「写临时→读回→独立复判」是两道独立门 | **推翻成立**。`profile_store.cpp:266` 的 `roundtrip != json_text` 先命中并 return ⇒ `:271` **在任何输入下都不可达** |
| **D5** | 令全部 provider 失败、`correctness_test="oracle:fail"`、证据 `executed>0 ∧ 有首次不符`（即自认**代码性缺陷**） | 「代码性 = 真缺陷 = 必须判红」（`profile_gen.h:40` 逐字） | **推翻成立**。`oracle_fail_evidence_violation`（`profile_gen_v2.cpp:379-399`）只对 `kUndetermined` 判红；`kCode` 返回 `""`（合规）⇒ **R-53 整套分类器造出了「代码性」标签，却没有任何判据对它采取行动**，profile 照常通过 `verify_profile_v2` 并落盘 |
| **D6** | `bench_memory(n, reps=0)` | 公开 API 参数受控 | **推翻成立**（推理，未运行）。`bench_harness.cpp:144-147` 的 `med_ns` = `v[v.size()/2]`，空 vector 下 `v[0]` 越界 |
| **D7** | `block_candidates(l2, elt_size=0)` | 公开 API 参数受控 | **推翻成立**（推理）。`:114` `l2_bytes / (elt_size*8)` ⇒ 整数除零 SIGFPE |
| **D8** | `advise_kernel_v1("", kid, "io", "small", limits)` | `worker_advisor.h:77` 声称 chain 是「机器可查」的选择链 | **推翻成立**。chain = `no_profile\|dynamic_multithread\|affinity\|fallback_io_tiny_single_worker\|` —— **同一条链先宣称动态多线程、再宣称单 worker** |
| **D9** | 把 `profile_gen_v2.cpp:565` 的 `3, 7` 改成 `5, 11` | 「报告申报的测量协议是真的」 | **推翻成立**。报告仍写 `warmup:3/samples:7`（`bench_report.cpp:225-226`），复读仍绿（`:352-355`），**报告将谎报它自己没做过的协议** |
| **D10** | 把 `out.production_profile_touched` 置 `true` 后重跑 generate | 「该字段被真实接线」 | **推翻成立**。`bench_report.cpp:315` 写的是**另一个字面量** `false`，与 `out.` 无关；JSON 仍 false，复读仍绿 |
| **D11** | `profile_store.cpp` 读路径：同一 target 连续两次 load | 「load 幂等」 | **部分推翻（如实界定）**。第一次 `rejected` + 精确 reason 并把文件改名为 `.rejected-<utc>`；第二次 `path_exists` 为假 ⇒ 返回 **`missing` + 无 reason**。**两次 `valid` 都是 false（未洗掉判定），被洗掉的是归因** |

### 关于「自愈判据/锚」的专项结论（诚实否定）

我按「读路径文件集合 ∩ 写路径文件集合」逐条列了全集：
- 读：`target`（`profile_store.cpp:295/304`）、`target.tmp-<pid>-<rand>`（`:265`）、`parent` 目录枚举（`:243`）
- 写：`target`（`:280`）、`target.tmp-...`（`:254/256`）、`target.rejected-<utc>`（`:313/322/344/374`）

**交集 = {`target`, `parent`}。但 `target` 读出的内容只用于结构/身份/消费级校验，从未被当作「上次测量值/基线/期望值」。** 且运行期 `build_route_table_v1(..., live_rows=nullptr, ...)` 显式**不重测**。`target.tmp-...` 是唯一「本次写、本次读」交集，其复核即 D4：真门只有 `:266`，`:271` 恒真。

⇒ **本片不存在「读被本次执行覆写的文件当基线」型自愈锚。** 最接近的 D11 洗掉的是归因而非判定。`profile_gen_v2.cpp:222` 的全局 `lcg_state` 是**可复现性缺陷**（构成自愈的必要条件），但 oracle 的 `ref` 与喂给 kernel 的输入同源，纯结构性缺陷不会翻转红绿 —— 我不把它夸大成已证实的自愈锚。

---

## 6. 盲复算（遮蔽既有判定，独立取证）

口径：先只读 `片清单-权威版.yaml` 取成员与行数，再对每份文件独立取证，**不打开任何 `审稿-RR*` / `审稿-R2-*` / `审稿-R3-*` / `审稿-P1-*` 作为判定依据**；子代理报告在**我自己读完 45 份之后**才用于交叉复核。

- **成员数**：清单 45 = `git ls-files` 45 = 磁盘 45 ⇒ **一致**
- **总行数**：清单 8389 = 我逐份 `wc -l` 累加 8389 ⇒ **一致**
- **判定一致性**：对本片 5 条阻断，其中 BL-1/BL-2 是我读完 `cpu_routing.cpp` 与 `bench_report.cpp` 后**独立**得到的；BL-3 我先在 `baseline_provider.cpp:429` 发现，再在 `avx2/avx512` 复现；三处 `run_banded` 我**全部自己读完**。⇒ **一致，不偏松也不偏严**
- **偏松处（我主动加严）**：子代理 C 的 S6/S7（`quota_signature` 空串 / `available_logical_cpus` 默认 1 vs 0 ⇒ 恒红门）我**复核后降级**——`hardware_inspect.cpp:317/329` 证明这两个键**恒被写出**，默认值不可达，故它们是「理论可达、生产不可触发」，只值建议。子代理 B 的 C0-4（`workers` 记请求值）我确认成立但把它与 BL-3 的预算破口连成一条因果链，因为后者的危害更大。
- **偏严处（我主动放宽）**：子代理 C 的 D5 断言「ISA 变体链路生产永久静默失效」——触发点在 `commands.cpp:2639`（**片外**），我只把**吞掉点**（`profile_gen_v2.cpp:446/450`）记为本片缺陷，把路径错误记为待前台裁决的跨片线索，未按本片阻断计。子代理 E 的 S2（`backend_table.inc` 六处 F64）我确认成立，但同时发现**同库 `profile_gen.cpp:134` 自证 `"fp32"`**，按「仓内已有直接反证」定级。
- **我独立推翻的子代理结论（否决清单）**：
  1. 否决「`profile_gen.h:24-33 RawCandidate` 未初始化」—— `:28-32` 有默认成员初始化，我亲自核对
  2. 否决「`capability_detect.c:220-239` brand 缓冲区溢出」—— `off < sizeof(b)` 在每次写入前守卫，无溢出；真实问题是末位字符被 `b[47]='\0'` 截掉
  3. 否决「`avx512_provider.cpp:86` 非 GCC 下 cap_gate 无 ISA 保护 = 缺陷」—— R-60 已把计算面移到零旗标门面 TU，MSVC 侧确实安全
  4. 否决「`avx512_provider.cpp:80` 引的检查脚本已删」—— `eng/tests/cpu/avx512/check_avx512_illegal_instr.py` **实测存在**
  5. 否决「`capability_detect.c` 未初始化栈外泄进 JSON」—— `%s` 在 `v[12]='\0'` 处截断；真实问题是结构体字段含未初始化字节
  6. 否决「`capability.schema.json` 与 `cap_bit_name` 词表不匹配」—— 13 个名字**逐条对齐**
  7. 否决「`host_services.cpp:77` CAS 用 relaxed 是数据竞争」—— 纯计数器，relaxed 足够
  8. 否决「`baseline_provider_v1.h:12` 引的 `CPU_BACKEND_ARCH.md` 已删」—— 实测存在
  9. 否决「`avx_backend.cpp` 注释不实」—— 逐条核实全部属实
  10. 否决子代理 C 的 S6/S7 恒红门定级（见上「偏松处」）

---

## 7. 子代理派发记录

派发 **5 个**，全部只读、零 git 写、未编译、未读 `/tmp/acsd_g08/`，全部在我自己读完 45 份**之后**用于交叉复核。

| 代理 | 范围 | 覆盖 | 交付 |
|---|---|---|---|
| A 后端加载与选路 | `cpu_routing.*`、`backend_loader.*`、`backend_table.inc`、`cpu_features.*`、`hardware_inspect.*`、`host_services.cpp` | 1298/1298 | **未交付完整报告**（末条消息为「shell 在破坏我的字符串，改用 grep 工具」）。**我未采信其任何结论**；该范围全部由我自己重读并独立取证 |
| B 基准框架/报告/建议 | `bench_harness.*`、`bench_report.*`、`worker_advisor.*` | 1298/1298 | 已交付，含 15 个反例、8 条否决 |
| C 档案生成与存储（两次独立派发） | `profile_gen_v2.cpp`、`profile_gen.cpp/.h`、`profile_store.cpp/.h` | 1805/1805 ×2 | 已交付两次，两次结论高度一致；含 D1–D6 反例、10 条否决 |
| D CPU 能力探测与 ISA 提供者 | 3 个 provider + 3 个 kernels + `capability_detect.c` + 3 个能力头 + schema + `cpu/common/README.md` | — | **未交付完整报告**。**我未采信其任何结论**；该范围全部由我自己重读并独立取证 |
| E 后端实现 + 登记面 | 12 份（含 `PENDING.md`/`README.md`） | 822/822 | 已交付，含 13 个反例、12 条否决 |

**我如何逐条复核**：把每条带 `文件:行` 的结论回到原文，用 read 打开该行区间逐字比对。**采纳 31 条、修正定级 4 条、否决 10 条**（否决清单见 §6）。典型修正：
- 子代理 C 的 B1「ISA 链路永久失效」——触发点在片外，我**降级为跨片线索 + 本片吞掉点缺陷**，不按本片阻断计。
- 子代理 C 的 S6/S7 两条「潜伏恒红门」——我读完 `hardware_inspect.cpp:317/329` 后**降级为建议**（默认值不可达）。
- 子代理 E 的 S2（F64 标签）——我补上了同库 `profile_gen.cpp:134` 的直接反证，证据链更硬，维持须修但理由更准。
- 子代理 B 的 C0-2（`production_profile_touched`）——我按 `bench_report.h:101`「恒 false（结构性；verify 强制）」的**诚实声明**把定级从阻断降到建议，但保留了「该字段在运行期零信息量」的准确表述。

**零 git 写**：本次会话未执行任何 `git add/commit/checkout/reset/stash/git rm`；未修改任何仓内文件（唯一写入为本交付件）。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
# 0) 基线
git -c core.quotepath=false log -1 --format='%H %s'          # 实测 1fa477a7…（派单写 850a9ede，以实测为准）

# 1) 片成员与行数（口径：git 跟踪文件数 / wc -l 物理行）
awk 'NR>=3162 && /^- 片号:/{if(NR>3162) exit} NR>=3162' \
  "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" \
  | grep -o '"lib/infrastructure/benchmark/[^"]*"' | tr -d '"' > /tmp/inf_bench_files.txt
wc -l < /tmp/inf_bench_files.txt                              # 45
git -c core.quotepath=false ls-files lib/infrastructure/benchmark | wc -l   # 45
while IFS= read -r f; do wc -l < "$f"; done < /tmp/inf_bench_files.txt | paste -sd+ | bc   # 8389

# 2) BL-5 登记面三个计数 + 两项存在性断言
git -c core.quotepath=false ls-files lib/infrastructure/benchmark | wc -l                    # 45（PENDING.md:8 称 38）
git -c core.quotepath=false ls-files lib/infrastructure/benchmark/backend_host | wc -l      # 30（PENDING.md:9 称 27）
git -c core.quotepath=false ls-files lib/infrastructure/benchmark/cpu | wc -l                # 13（PENDING.md:10 称 10）
ls lib/infrastructure/benchmark/README.md                                                    # 存在（PENDING.md:12 称「缺」）
grep -c 'readme:' docs/engineering/MODULE_MAP.md                                              # 0（PENDING.md:12 称「已声明」）
ls eng/tools/quality/check_module_map.py; ls eng/tools/quality/check_variant_isa_disasm.py  # 均 MISSING
sed -n '8,12p' lib/infrastructure/benchmark/PENDING.md

# 3) BL-2 violations 的两个消费者，一个遵守一个不遵守
sed -n '2640,2650p' lib/infrastructure/cli/commands.cpp     # 遵守：!pb.violations.empty() → INTERNAL
grep -n 'violations' lib/infrastructure/benchmark/backend_host/bench_report.cpp || echo "bench_report.cpp 零引用 violations"

# 4) BL-1 生产选路只认档案自报字段
sed -n '514,523p' lib/infrastructure/benchmark/backend_host/cpu_routing.cpp
grep -n 'build_route_table_v1' lib/infrastructure/cli/commands.cpp     # 调用点
sed -n '325,330p' lib/infrastructure/cli/commands.cpp                   # live_rows=nullptr

# 5) BL-3 三 provider 的 acquire 丢返回值 / 无条件 release
grep -n -A1 'if (workers == 1)' lib/infrastructure/benchmark/cpu/*/src/*_provider.cpp
grep -n -A3 'executor->release' lib/infrastructure/benchmark/cpu/*/src/*_provider.cpp
sed -n '82,85p' lib/infrastructure/benchmark/backend_host/host_services.cpp   # fetch_sub 无下溢保护
sed -n '58,67p' lib/infrastructure/benchmark/backend_host/baseline_kernels_impl.inc
sed -n '62,67p' eng/tests/unit/cpu_backend_exception_test.cpp                 # exhaust_acquire 对 n==1 恒返回 0
grep -n 'budget_exhausted' eng/tests/unit/cpu_backend_exception_test.cpp

# 6) D1 筛掉真信号：聚合可选出非最小值
sed -n '136,148p' lib/infrastructure/benchmark/backend_host/bench_report.cpp
sed -n '73,79p'  lib/infrastructure/benchmark/backend_host/bench_report.h          # 声明的契约 best = min(median)

# 7) D2 break 位置
sed -n '673,687p' lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp

# 8) 自洽式断言四处
sed -n '264,277p' lib/infrastructure/benchmark/backend_host/profile_store.cpp      # :266 之后 :271 不可达
sed -n '204,216p' lib/infrastructure/benchmark/backend_host/profile_store.cpp      # 自身 commit 当期望值
sed -n '200,203p;313,317p;411,415p' lib/infrastructure/benchmark/backend_host/bench_report.cpp
sed -n '38,53p'  lib/infrastructure/benchmark/backend_host/baseline_backend.cpp    # 同帧抛并捕获
grep -n '"amd64"' lib/infrastructure/benchmark/backend_host/*.cpp \
              lib/infrastructure/benchmark/cpu/common/src/*.c \
              lib/infrastructure/benchmark/cpu/common/schemas/*.json

# 9) 恒假守卫与恒真门
sed -n '161,171p' lib/infrastructure/benchmark/backend_host/bench_harness.cpp      # acc == 1234.5678
sed -n '240,244p' lib/infrastructure/benchmark/backend_host/worker_advisor.cpp     # worker1_allowed
sed -n '73,75p'  lib/infrastructure/benchmark/backend_host/bench_report.cpp       # is_heavy
sed -n '68,81p'  lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp       # kSpecs workload 全非空
sed -n '7,9p'    lib/infrastructure/benchmark/backend_host/worker_advisor.h && sed -n '220,224p' lib/infrastructure/benchmark/backend_host/worker_advisor.cpp

# 10) precision 标签矛盾 + ISA 判据不等价 + 退役零消费者
grep -n 'ACS_PRECISION_F64' lib/infrastructure/benchmark/backend_host/backend_table.inc
sed -n '134p' lib/infrastructure/benchmark/backend_host/profile_gen.cpp
sed -n '30,31p' lib/infrastructure/benchmark/backend_host/cpu_features.h && sed -n '88,89p' lib/infrastructure/benchmark/cpu/avx512/include/acsd/cpu/avx512_provider_v1.h
for s in advise_kernel_v1 generate_benchmark_report load_profile_checked_v1 generate_profile_json; do
  echo "== $s"; grep -rln "$s" lib eng | grep -v 'benchmark/backend_host'; done

# 11) 悬空引用
grep -n '15_CPU_PROVIDER_AND_RESOURCE_STANDARD' -r lib/infrastructure/benchmark
grep -n 'engineering-evidence/prerelease-v5' -r lib/infrastructure/benchmark
ls -d 实验/engineering-evidence/prerelease-v5          # 今日存在，但正是最新裁决要移除的归档层
grep -n 'lib/include/acsd/cpu/capability_v1.h' lib/infrastructure/benchmark/cpu/common/README.md   # :13 错 / :56 对
```

---

## 9. 给前台的处置建议（按危害排序）

1. **BL-1 + BL-2 必须一起裁决**：它们共同决定了「这份档案到底有多可信」。若生产只信任档案，则 `profile_gen_v2` 的任何缺陷都没有第二道门；若要保留第二道门，`cpu_routing.h:108-113` 的契约要改（live_rows 至少要做一次 spot benchmark）。这是**架构决策，不是逐点可修的缺陷**，建议单独立项。
2. **BL-3 立即修**：三个 provider 的 `run_banded` 各加一句 `if (acquire(1) == 0) ... else 走串行且不 release` 即可；同时把 `host_release` 改为无下溢版本。
3. **BL-4 立即修**：`.inc` 的四处 `.count` 比较换成 `spans_ok`。
4. **BL-5 立即修**：`PENDING.md` 全文重写或删除（同目录 `README.md` 是正确的那一份，可直接作目录说明正本）。
5. **D5（`kCode` 不判红）单独立项**：`profile_gen.h:40` 声明「代码性必须判红」，实现只对 `kUndetermined` 判红。R-53 造出了一个没有牙齿的分类器。
6. 待前台执行的实测项（本报告未运行任何二进制）：`profile_gen_v2` full 模式的峰值 RSS（我按 `build_inputs` 推算 `upm-spmv` large 单候选约 870MB，**纸面推导未实测**）；以及 `build_inputs` 的 `float` 行指针累加在 `upm-spmv` large（N=2^24，total≈4N ≫ 2^24）下的**静默退化**——我推演它会让 CSR 行指针停滞、约 2/3 行变空行、kernel 与 oracle 同源因而 `oracle:pass` 全绿，但**未实测**，请前台以 `quick`/`full` 实测确认后再定级。