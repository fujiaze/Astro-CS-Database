# 审稿-P1-INF-scheduler-003（G08-05 对抗审稿 第 1 遍 · 生产源码）

- **片号**：`INF-scheduler-003`
- **层**：`lib/infrastructure/scheduler`
- **基线**：仓 `/workspace/Astro CS Database`，**实际 HEAD = `f4a2cf21`**（交付要求写的 `850a9ede` 与仓内不符，如实记录；本片结论基于 `f4a2cf21` 工作树）
- **审稿人**：P1 审稿代理（红队姿态）
- **零写入确认**：未 add / commit / checkout / reset / stash；未 `git rm --cached`；未编译、未跑 ctest/pytest/构建/任何仓内二进制；未修改任何仓内文件（唯一写入为本交付件）；未读 `/tmp/acsd_g08/`

---

## 1. 读完了吗

### 1.1 口径声明

- **行数口径**：`wc -l`（含末行无换行时按行计），**按文件去重**；下表 13 份成员实际行数合计 **4026**，与 `片清单-权威版.yaml:3476-3478` 登记的「成员份数 13 / 实际行数 4026」**逐份精确相符**（无缺文件、无多余文件）。
- **覆盖率双口径**：
  - **口径 A（我亲自读原文）**：以 `read` 工具逐行读过的行数 ÷ 4026。
  - **口径 B（含子代理逐行核验）**：我亲自读 ∪ 子代理逐行读完。

### 1.2 逐份读出行数

| # | 成员文件 | 登记行数 | 我亲自读 | 亲自覆盖 | 子代理逐行读完 |
|---|---|---|---|---|---|
| 1 | `src/memory_pressure.cpp` | 656 | 656 | 100% | ✅ (两代理) |
| 2 | `budget.py` | 641 | 641 | 100% | ✅ (两代理) |
| 3 | `src/canonical_hash.cpp` | 516 | 516 | 100% | ✅ |
| 4 | `src/normalize_workflow.cpp` | 503 | 110 | **21.9%** | ✅ |
| 5 | `src/mosaic_window.cpp` | 369 | 120 | **32.5%** | ✅ |
| 6 | `src/pipeline.cpp` | 349 | 349 | 100% | ✅ |
| 7 | `src/block_flow.cpp` | 327 | 327 | 100% | ✅ |
| 8 | `src/artifact.cpp` | 190 | 190 | 100% | ✅ |
| 9 | `src/module.cpp` | 179 | 179 | 100% | ✅ |
| 10 | `src/checkpoint.cpp` | 134 | 134 | 100% | ✅ |
| 11 | `core/phase_lifecycle.README.md` | 70 | 70 | 100% | ✅ |
| 12 | `src/memory_budget.cpp` | 57 | 57 | 100% | ✅ |
| 13 | `README.md` | 35 | 35 | 100% | ✅ |
| | **合计** | **4026** | **3384** | **84.1%** | **13/13 份** |

### 1.3 覆盖率结论

- **口径 A = 3384 / 4026 = 84.1%**。完整读完 **11/13 份**。
- **口径 B = 4026 / 4026 = 100%**（子代理对全部 13 份完成逐行阅读，无遗漏）。
- 另有 **4 份被双读**（memory_pressure.cpp / budget.py / artifact.cpp / module.cpp），构成独立二次取证。

### 1.4 ⚠️ 未由我亲自读到行尾的部分（如实列出）

| 文件 | 未亲自读到的行段 | 行数 | 缺口性质 |
|---|---|---|---|
| `src/normalize_workflow.cpp` | 1–239、345–503 | 393 | **我未亲读**；结论来自子代理 + 我对 240–344 的定点复核 |
| `src/mosaic_window.cpp` | 1–169、290–369 | 249 | **我未亲读**；结论来自子代理 + 我对 170–289 的定点复核 |

**这 642 行（15.9%）不是「读完了」**。凡只由子代理单侧主张、本人未复核的条目，本报告一律在「证据强度」列标注为「单侧」，不与「双侧」混同。

### 1.5 为建立判据基线而额外读的正本（非本片成员，不计入覆盖率）

`docs/ACSD_DESIGN.md`（定点）、`docs/engineering/{SCHEDULER_CONTRACT,CONCURRENCY_STANDARD,ERROR_HANDLING_STANDARD,NUMERIC_STANDARD,MANIFEST_VERIFY_V1}.md`、`eng/packaging/config/runtime_resources.json` 与 `runtime_resources_generated.h.in`、`eng/contracts/schemas/scheduler_probe_event.schema.json`、`lib/include/acsd/core/{memory_pressure,block_frame,checkpoint,module}.h`、`lib/infrastructure/scheduler/src/block_frame.cpp`（定点）、`eng/tests/runtime/`（目录清点）、`run/GOVERN-08/工作包-GOVERN-08原件/standards/04_对抗性审稿规范.md`。

---

## 2. 本片判定

## **阻断**

> 判定理由：本片同时存在 ① 生产路径上的**有符号整数溢出 UB**、② 三处 **fail-open（失败信号被吞）**、③ 一个**恒真门**且其打印标签与所验相反、④ 一个**零阈值默认构造即活锁**的地雷。任一条单独即足以阻断合并。
>
> 同时如实记录：本片**质量分层明显**——`memory_pressure.cpp` / `memory_budget.cpp` 是全片最好的实现（显式 fail-open 标注、「没丢也是一个决策」也落盘、溢出安全整数式），`pipeline.cpp` 严谨度次之。**阻断项集中在 canonical_hash / checkpoint / block_flow / budget.py / 头文件默认值**，不是全片性劣化。

### 最重 3 条

| 排名 | 问题 | 位置 | 为什么是阻断 |
|---|---|---|---|
| **1** | **生产哈希路径上有符号整数溢出 UB，且"防溢出"守卫放在溢出之后** | `src/canonical_hash.cpp:310,313,315` + 守卫 `:317` | 构造畸形/损坏 FITS（`NAXIS1=2^40, NAXIS2=2^40`）即可让 `npix *= n` 越过 `long long` 边界。`if (nbytes < 0) return false;` 位于溢出**之后**，对 UB 不构成防线（编译器可假设不溢出）。该文件是**规范产品哈希的生产实现**，是全部产品完整性判据的根。 |
| **2** | **两条独立的 fail-open：已提交节点的产物被静默清空；未声明消费者被静默放行且整轮报 `ok=true`** | `src/checkpoint.cpp:82-88`（对 `:44-47` 半成品守卫的旁路）；`src/block_flow.cpp:213-214` + `src/normalize_workflow.cpp:293` | `commit_node` 明写「半成品拒绝」并拒绝空产物，`replay_same_run` 在 key 已存在时**直接覆写 `artifact_ids` 且不查空**，把被守卫禁止的状态造了出来。`consume()` 的 3 个失败原因（块不存在/状态非 CREATED/**未声明消费者**）返回值在两处调用点分别被丢弃或与「块仍存活」混同。两者都会让 resume/生命周期违规以**成功**面貌通过。 |
| **3** | **恒真门，且打印标签声称验证了它并未验证的事** | `budget.py:565`（被 `budget.py:429` `max(0.0, dsum)` 保证恒真） | `check(s2["d_thread_sum"] >= 0.0, "per-thread cpu delta sampled")` 在**一个字节都没采到时**（Windows 下 `_read_thread_cpu` 返回 `{}`）仍打印 `[ok] per-thread cpu delta sampled`。`04_对抗性审稿规范.md:26`：「恒真、空断言、不读真实对象的判据判无效」。更严重的是源内 docstring `:346-350` 自认该门「改松以适配降级而保持绿」——**诚实只在注释里，PASS 输出里没有**。这正是负责人裁定要抓的形态。 |

---

## 3. 逐文件清单

### 3.1 `src/canonical_hash.cpp`（516 行）— **阻断**

**读了什么**：全文。连带读 `block_frame.cpp` 无关部分、`runtime_resources.json` 配置口径。

**看到什么 / 判定**：

| 判定 | 位置 | 问题 |
|---|---|---|
| 🔴 阻断 | `:310,:313,:315`（守卫 `:317`） | **有符号溢出 UB**。`row*rows*gcount+pcount`、`npix*=n`、`npix*bpp*gcount` 全为 `long long` 运算；`if (nbytes<0) return false` 在溢出后，对 UB 无效。 |
| 🔴 阻断 | `:269` | `naxis.assign(n>0?n:0, 0)`，`n` 来自**文件内容**且无上界。`NAXIS = 1000000000000` ⇒ 申请 8 TB 容器 ⇒ `bad_alloc`。该异常从 `canonical_product_hash_file`（返回 `CanonicalHashResult`，无异常契约）逃逸到调用方。 |
| 🟠 须修 | `:192-214` `prune_json` | **筛掉真信号**：排除键按**键名在任意深度**匹配。`{"stars":[{"sha256":"aaa"}]}` 与 `{"stars":[{"sha256":"bbb"}]}` 剪枝后同为 `{"stars":[{}]}` ⇒ **两个不同产品规范哈希相同**。「产品完整性哈希」被自身的排除表消解。缓解：`excluded_keys_hit`（`:469`）有记录，但**哈希本身已碰撞**。 |
| 🟠 须修 | `:343-346` `char tail[96]` | `snprintf` **静默截断**（非溢出）。固定开销 85 字符，`%zu` 只剩 **11 位**可用 ⇒ 数据单元 **≥ 10¹¹ B（100 GB）** 时 `bytes=` 被截断成错值，而 `data_hash` 仍覆盖完整单元。`ERROR_HANDLING_STANDARD` V4 判静默截断为红。 |
| 🟡 建议 | `:386,:413` vs `:94-101` | 分支选择用**大小写敏感**字节比较（`head != kSimple`），而解析器 `fits_keyword()` `:99` 做 `toupper` ⇒ 小写关键字 FITS 被**静默降级为 raw**，得不到 FITS 规范化。检出与解析口径不一致。 |
| 🟡 建议 | `:143-153` | JSON 合法超大整数（> 2^63）落入 `%.17g` 分支 ⇒ 精度丢失 ⇒ 相邻大整数碰撞。 |
| ✅ 通过 | `:41-48` 排除表、`:127-141` 控制字符转义（**比 `artifact.cpp:12` 与 `memory_pressure.cpp:73` 严格**）、`:386-408` 单遍流式、`:63` 未命中即返回 false | 这些是真 fail-closed。全片最佳实践之一。 |

**证据强度**：双侧（我亲读 + 子代理独立）。

---

### 3.2 `src/checkpoint.cpp`（134 行）— **阻断**

| 判定 | 位置 | 问题 |
|---|---|---|
| 🔴 阻断 | `:82-88` | **fail-open（自洽式旁路）**。`:44-47` 刚以「半成品拒绝」拒绝空 `artifact_ids`；`:82` 的 `if (index_.count(k))` 分支**不查空**直接 `e.artifact_ids = artifact_ids` 并 `return success`。同一不变量在姊妹路径被绕过。 |
| 🟠 须修 | `:53-55` | `commit_node` **不查重**：同 `(node_id, scope)` 重复提交产生两条 `entries_`，`committed_nodes()` `:70-76` 遂把同一节点返回**两次**（返回的是 list 而非 set）。 |
| 🟡 建议 | `:18-29` | `begin()` 的「run_id 中途变更」守卫被 `if (!entries_.empty())` 限定 ⇒ **首次 commit 之前** `run_id` 可被静默改绑并返回 success。 |
| ✅ 通过 | `:93-132` `validate_resume_inputs` | 逐项 fail-closed：`store.get` 失败、`validate` 失败、`expected_schema_id` 空、schema/version 不符、`expected_sha256` 空、content hash 不符 ⇒ 全部返回 `Result::fail`，带 `ErrorDomain::DATA`。**全片最干净的函数之一**。 |

**构造的反例（推翻「半成品拒绝」不变量）**：
```
begin("r1"); commit_node("n1", {"a1"});      // entries_=[{n1,[a1]}], index_[n1]=0
replay_same_run("n1", {});                    // 命中 index_ ⇒ 直接覆写 ⇒ [n1,[]]
is_committed("n1") == true，但该已提交节点产物为空
```
⇒ **推翻** `checkpoint.cpp:43-47` 声称的不变量。**证据强度**：双侧（我亲读构造，子代理同侧）。

---

### 3.3 `src/block_flow.cpp`（327 行）— **阻断**

| 判定 | 位置 | 问题 |
|---|---|---|
| 🔴 阻断 | `:213-214` + `block_frame.cpp:191-206` | **fail-open**。`consume()` 有 **3 个失败返回点**（`:193` 块不存在 / `:195` 状态非 CREATED / `:197` **未声明消费者**），本文件把 `false` 单一解释为「未销毁」，**从不判失败**。`:144` 注释「未声明读 = fail-closed」实际只对**缺块**成立，对**未声明消费者**不成立。 |
| 🟠 须修 | `:87-93` `set_input` | `element_count` **零范围校验**即存入，后续 `:132` 送入 `block_frame.cpp:153-155`；那里的 `element_count * dtype_size` 可在 `size_t` 上回绕，**回绕后反而通过 `kMaxBlockBytes` 检查** ⇒ 声明巨大元素数可绕过帧上限（跨片问题，`block_frame.cpp` 属 `INF-scheduler-002`，**入口在我片**）。 |
| 🟠 须修 | `:244` | 第 3 处误植 FNV offset basis（本片 4 处之一）。 |
| 🟠 须修 | `:277-325` `parse_stage_block_flow_spec` | **异常逃逸**：`:281-286` 只护 `json::parse`；`:287` 只查 `contains("nodes"/"blocks")` **不查类型**。`:294` `n.value("stage", ...)` 对非对象抛 `type_error.306`；`:300-301`、`:314`、`:317` 同样无保护。函数签名是 `bool + error*`，却抛异常。 |
| 🟠 须修 | `:245-246` vs `:243` | 注释称产品块「延长到 `StageBlockFlow` 生命期」，但每个 `run_unit` 开头无条件 `product_names_.clear(); products_.clear();` ⇒ 上一单元产品被抹，跨单元产品**不可能累积**。注释与行为相反。 |
| 🟡 建议 | `:273` | `sink_->flush()` 返回值丢弃（探针写失败零信号）。 |
| ✅ 通过 | `:159-179` | 新增块**名字级**校验（不只比数量），且 `:167-168` 先排序 ⇒ 逻辑正确。`:109-112` 构建期图校验 fail-closed。 |

**构造的反例（推翻「未声明读 = fail-closed」）**：
```
块 x 由 n1 生产，spec_.consumers["x"] = ["n3"]（只有 n3 消费 x）
节点 n2 声明 reads=["x"]
:145-151  frame.find("x") 命中（块存活）⇒ 通过「未声明读」检查
:154      n2 执行（已读到 x）
:213      consume("x","n2") ⇒ remaining_=["n3"] 无 "n2" ⇒ block_frame.cpp:197 返回 false
:214      destroyed=false ⇒ 无计数、无探针、无 error
:270      out.ok = true      ← 生命周期违规以成功面貌通过
```
⇒ **推翻** `block_flow.cpp:144` 的 fail-closed 声称。**证据强度**：双侧（我亲读 + 子代理独立）。

---

### 3.4 `budget.py`（641 行）— **阻断**

| 判定 | 位置 | 问题 |
|---|---|---|
| 🔴 阻断 | `:565`（`:429` 保证） | **恒真门 + 标签说反**。`d_thread_sum = max(0.0, dsum)` ⇒ `>= 0.0` **不可能失败**；Windows 下 `_read_thread_cpu` 返回 `{}` 时仍打印 `[ok] per-thread cpu delta sampled`。`04_对抗性审稿规范.md:26` 判无效。 |
| 🔴 阻断 | `:151-162` | **fail-open（哨兵重载）**。`:155` `ask = avail if want == 0 else want` ⇒ **「要 0 个线程」被解释为「拿走全部可用核」**。算例：`cores=8, leased=0, request_lease("A", 0)` ⇒ 授予 **8**，且**不计入 `_denied`**；随后所有正常请求全被拒。§10.4「单进程唯一预算源」退化为建议性。 |
| 🟠 须修 | `:164-166` | `release_lease` 无租约身份 + `max(0, ...)` 钳位 ⇒ **任何模块可重复释放他人租约并静默"成功"**，释放后 `avail` 虚增 ⇒ 超额订阅。 |
| 🟠 须修 | `:475` + `:503` | **筛掉真信号**。只取**最后一个瞬时斜率** `samples[-1]["rss_growth_mb_per_s"]`，却用 `> 32.0` 判 `memory_growth`。前 9 次采样以 100 MB/s 泄漏、第 10 次持平 ⇒ 终值≈0 ⇒ **门全绿**。 |
| 🟠 须修 | `:374-384` + `:470` | **量纲错**。`_read_iowait_seconds()` 读 `/proc/stat` 的**全机聚合** iowait，却以 `100*iow/本run墙钟` 当作**进程级** `io_wait_percent` ⇒ 64 核机上被放大约 64 倍，且可因**别的进程**的 I/O wait 触发 finding（`:507`）。 |
| 🟠 须修 | `:454-455`、`:625`、`:636` | **零采样仍退出 0**。`seconds<=0` ⇒ `samples` 空 ⇒ 返回全 0 指标；`main` 无条件 `return 0`。且 `:495` 会为 `cpu_mean_percent=0.0` 产出「cpu mean 0.00% < 85%」——把**从未采到**误标为**利用率低**。 |
| 🟠 须修 | `:86-105` | `missing_keys()` 的 `:88` `if not hasattr(self,k)` 是**死代码**：`HeavyRunMetrics` 是全字段带默认值 dataclass，`hasattr` 恒真。且 `:521` 输出的 `missing_metrics` **从不参与裁决**（`hard_fail` 恒 False）。 |
| 🟠 须修 | `:495,:499,:503,:507` | 5 个阈值（85.0 / 2 / 10.0 / 32.0 / 50.0）硬编码，**无推导、无配置面**，违反 AGENTS.md §6「需标定的移入配置」与本项目自订 `runtime_resources_generated.h.in:4`「实现侧不得再出现字面量阈值」。 |
| 🟡 建议 | `:108-133` | `ProcessBudgetRegistry._instance` 类级单例**无锁**创建；`reset_for_test()` 不复位 `_instance`。 |
| 🟡 建议 | `:119-123` vs `runtime_contract.h:208` | Python 版是 C++ 版的**平行复制品**，`selftest` 只自证副本；`:13-14` 自称「非自证」——**实际效果相反**。 |
| ✅ 通过 | `:174-179`, `:570-571`, `:589-592` | 注释**主动自陈**旧实现曾「静默返回 0」并使门「无判别力的绿」；负例（把 `_IS_WINDOWS` 置真但不替换实现，断言降级返回 0 且不崩）在 Linux 上**确有判别力**。这是全片少见的、注释与机制同向为善的例子。 |

---

### 3.5 `src/memory_pressure.cpp`（656 行）— **须修（地雷级）**

**看到什么 / 判定**：全片质量最高的实现。`:298` 注释**明确自称 "fail-open 到无治理"**（诚实，与配置文件的 "fail-closed" 标签冲突，见 §4）；`:387-393` 把「**没丢**」也落盘成 `evict_no_candidate` 并写明理由（「没丢也是一个决策，必须能解释」）；`:54-71` 是真溢出安全整数式；`:339/:348/:384` 对 dwell 做 `max(1,·)` 钳位。**不是**「检查通过」——以下问题由独立推导得出：

| 判定 | 位置 | 问题 |
|---|---|---|
| 🔴 阻断（地雷） | 头 `:108-114` + 本文件全篇 | **零阈值默认构造 ⇒ 永久 HIGH 活锁**。头文件 7 个阈值全默认 0，注释「0 = 未配置 ⇒ 由配置层填默认」，但**本文件无任何一处实现该兜底**。⇒ `:334` `pct >= high_percent(=0)` **恒真** ⇒ 第 1 采样即 HIGH+`STOP_DISPATCH`；`:384-385` `th=max(1,0)=1` 使 `((high_ticks-th)%th)==0` **恒成立** ⇒ **每采样丢一帧** ⇒ 并发度永久钉死 1 + 帧无限丢弃重跑。生产当前安全**纯属** `pressure_policy_from_config()` 全仓仅 1 个调用点；`.h.in` 对 p3/queue/cpu_budget 都写了 `static_assert` 值域守卫，**唯独内存压力 7 个键一个都没有**。 |
| 🔴 阻断 | `:210-214` | **台账落盘失败被 `(void)` 丢弃**。`flush_locked` 失败时**保留缓冲不清**（`:168/:174`）⇒ 磁盘满时每次事件都失败重试 ⇒ `ledger` **无限增长** ⇒ **在负责管内存的组件里制造内存泄漏**，同时 §8.3:612 证据面全丢且零迹象。唯一可见信号是无人读的 `status_json` `ledger_pending`（`:634`）。 |
| 🟠 须修 | `:510-545` `choose_victim` vs `:131-156` `evict_one_locked` | **淘汰选择器分叉**。公开 API `choose_victim` 是 **2 键**（stage_index↑, ticket↓），**不过滤 `ended`**，**不更新 `frame_evictions`（防颠簸）**；生产活路径是 **3 键**（主键=本帧身份已丢弃次数）。⇒ `:126-129` 声明的防颠簸保证只存在于活路径。且子代理实测 `choose_victim` **生产零调用者**（仅 3 处单测）⇒ 公开 API 一旦被调用即静默绕过保证。 |
| 🟠 须修 | `:130` vs `:524-531` | 注释「候选 = 未越过安全点、**未 ended**、未被选过」——`choose_victim` **不查 `ended`**；`InFlightFrame::ended`（头 `:140`）**全仓无任何写入点**（子代理 grep 实证），`end_frame` `:491` 直接 erase ⇒ 「未 ended」半句恒真。 |
| 🟠 须修 | `:73-82` `json_escape` | 只转义 `"` `\` `\n`；**不转义 `\r` `\t` 及 0x00–0x1F** ⇒ 含制表符/回车的 `frame_id`/`stage`/`note` 会产出**非法 JSONL 行**，静默损坏台账证据面。 |
| 🟠 须修 | `:103,:304` `announced_unavailable` | 不可用事件**一生只登记一次** ⇒ 探针恢复后再次失败**静默无痕**。 |
| 🟡 建议 | `:100` + `:491` | `frame_evictions` 是无界 `std::map<string,uint32_t>`，`end_frame` **不清理** ⇒ 长 run 无界增长（管内存组件自身泄漏）。 |
| 🟡 建议 | `:55` | **悬空符号**：`kPressurePercentSaturated` 在注释里被引用，**任何头文件都不存在**；真实饱和常数是函数内局部 `cap`（`:69`）。 |
| 🟡 建议 | `:54` vs `:68` | 注释「溢出安全」**过度声称**：`:63-66` 只护 `rem`，不护 `whole*100`。rss ≥ 184.5 PB 时回绕（**物理不可达**）。 |
| 🟡 建议 | `:373-375` | `else if (before != s.level) { s.last.level = s.level; }` 是 **no-op**（`:363` 已赋同值），死分支。 |
| ✅ 通过 | `:122` `available()`、`:409/:438` 零副作用关闭路径、`:640/:649-654` `thread_local` + RAII `GovernorScope` | `g_current_governor` 是**线程局部 + 作用域保存/恢复**，**不是**进程全局单例，**不违反** RT-002 §2 不变量 2（避免误判）。 |

**构造的反例（推翻「0 = 未配置 ⇒ 由配置层填默认」）**：默认构造 `PressurePolicy{}` 后调 `step_locked` ⇒ 首采样即 `HIGH`+`STOP_DISPATCH`，其后每采样 `evict_one_locked()` 一次 ⇒ 并发永久 1 + 无限丢帧。**推翻**。**证据强度**：双侧。

---

### 3.6 `src/module.cpp`（179 行）— **须修**

| 判定 | 位置 | 问题 |
|---|---|---|
| 🟠 须修 | `:53` + 头 `:34` | `execution_class` **从不校验是否属于闭集** `{cpu_heavy, io, light}`；`:53` 是**裸字符串比较**。取 `"CPU_HEAVY"` / `"cpu_heavy "` / `""` 任一 ⇒「heavy+serial 拒绝」**静默失效**，而 `:165` 照样把错值写进导出索引。 |
| 🟠 须修 | `:58` | **ACR 禁令可绕**：`rfind("acsd.acr.", 0) == 0` 要求尾随点。`module_id == "acsd.acr"` 通过 `:19` 的 `"acsd."` 前缀检查，但**不匹配** `"acsd.acr."` ⇒ 「ACR 不接生产」禁令不触发。 |
| 🟠 须修 | `:156-177` | ① `if (out) *out = j.dump(); return true;` —— `out == nullptr` 时整份 JSON 已构造却被丢弃**仍返回 true**（声称成功而零产出）；② `j.dump()` 可抛，**无 try/catch**，异常从 `bool` 函数逃逸；③ 同 TU 内 `:87/:100/:129` 走 `Result<T>`，**两套错误模型并存**。 |
| 🟠 须修 | `:160-174` | `export_index_json` **丢弃 `ports`**（`:31-51` 判其为必填）与 `config_schema` ⇒ 导出的索引**无法回验、无法重建 descriptor**。 |
| 🟠 须修 | `:89,:100,:104,:118` | 注册/配置类错误一律报 `ErrorDomain::DATA`，应为 `CONFIG`；6 处失败**全为自由文本，无稳定 numeric_code**。 |
| 🟡 建议 | `:112-121` | 注册期 `factory()` 探针实例化验证，但 `create()` `:132` **再次调用工厂且不复验 descriptor** ⇒ 工厂若非纯函数（计数器/随机），注册期验证与生产实例**不绑定**。 |
| 🟡 建议 | `:112` | 每次注册额外构造并销毁一个实例（构造有副作用时有风险）。 |
| 🟡 建议 | `:27` | ABI 白名单硬编码 `{"c++17","c"}`。 |
| ✅ 通过 | `:14-83` 其余校验、`:96-123` `register_factory` | 空工厂 / 未先注册 / 重复工厂 / 工厂返回 null / descriptor 不匹配**全部 fail-closed**。 |

**构造的反例**：① `ModuleDescriptor{execution_class="CPU_HEAVY", parallel_ok=false}` 过 `validate` ⇒ `:53` 不触发 ⇒ **推翻**「heavy+serial 拒绝」。② `ModuleDescriptor{module_id="acsd.acr", …}` 过 `validate` ⇒ **推翻**「ACR 不接生产」。③ `export_index_json(nullptr)` 返回 true 而零产出 ⇒ **推翻** bool 返回反映成功。**证据强度**：我亲读构造，子代理独立同侧。

---

### 3.7 `src/pipeline.cpp`（349 行）— **建议**

**全片严谨度第二高的文件**（逐字段 `is_string()`/`is_object()` 守卫、`Result<T>` 单一错误模型、环检测正确、UNCONSUMED/UNPRODUCED_OUTPUT 双向 fail-closed）。

| 判定 | 位置 | 问题 |
|---|---|---|
| 🟠 须修 | `:119-120` | **异常逃逸**：`nj.value("inputs", json::object())` 在 `inputs` 存在但**非对象**（如字符串/数组）时抛 `type_error.306`；仅 `:58` 的 `json::parse` 被 try 包住 ⇒ 异常从 `Result<PipelineIR>` 函数逃逸。与同文件 `:101/:105/:110` 逐字段守卫的严谨风格自相矛盾。 |
| 🟠 须修 | `:156` | **静默默认**：`n.parallel = pit != rit->end() && pit->is_boolean() && pit->get<bool>()`。`resources.parallel` **缺失或非布尔 ⇒ 静默变 `false`**，无 error。全文件唯一没有「required」检查的资源字段（`class` 有 `:145`、`resources` 有 `:140`）⇒ 违「参数缺失时吞掉并继续」。 |
| 🟡 建议 | `:34-53` `to_json` | 往返丢失：`parse` 读的 `config_ref`（`:168-174`，含 `sha256` 绑定）**从不写出** ⇒ 载入再序列化后**配置绑定哈希消失**（provenance pin 丢失）。 |
| 🟡 建议 | `:289` | `coordinate` 比较**无 UNKNOWN 豁免**，而紧邻的 `schema`（`:276`）与 `unit`（`:283`）都有空/UNKNOWN 豁免 ⇒ 未声明 coordinate 的端口会被硬判 `COORDINATE_MISMATCH`；若枚举 0 值是 PIXEL，未声明端口**静默读作 PIXEL**。三处口径不一致。 |
| ✅ 通过 | `:94,:101,:105,:110,:140,:145,:181` | 全部关键字段均有显式类型/存在性守卫。 |

---

### 3.8 `src/artifact.cpp`（190 行）— **须修**

| 判定 | 位置 | 问题 |
|---|---|---|
| 🟠 须修 | `:50-68` vs `:49` | 注释称「FNV-1a 64 稳定 hash」，实则 offset basis = **`1469598103934665603` = 正确值 `14695981039346656037` ÷ 10**（**掉了末位 `7`**）。已用 `python3` 复算确认（`std//10 == code` 为 `True`）。⚠️ **重要限定**：该错误 basis 在**全仓 10 处一致使用**（含 I/O 层 `io_adapter.h:45`、`aio/io/src/io_adapter.cpp:53,97` 及测试），故**不是本文件的局部笔误，而是全仓统一约定**。后果是：与任何符合 FNV-1a 规范的独立校验器（含 `eng/tools/canonical_product_hash.py` 一类 Python 镜像）**逐位不一致**。本片命中 4 处：`artifact.cpp:51`、`block_flow.cpp:244`、`normalize_workflow.cpp:222`（注释直书「// FNV offset basis」）、`mosaic_window.cpp:188`。 |
| 🟠 须修 | `:95-119` vs `:163-188` | **往返丢失 provenance**：`to_json` 写 `coordinate`/`ownership`/`provenance`，`from_json` **一个都不读**。解析后 `coordinate`、`ownership` 及**整份 provenance**（`source_commit`/`pipeline_hash`/`config_hash`/`input_hash`）全部为默认构造值，而 `:185` 的 `validate()` **不检查这三项** ⇒ 静默降级通过。 |
| 🟠 须修 | `:62` + `:107` | `validate` 从不校验 `coordinate`；若枚举默认值为 PIXEL，`from_json` 后所有 artifact 的坐标系**静默变成 PIXEL**，而 `:87` 的单位校验恰好对 PIXEL 放行 ⇒ **无任何报错**。 |
| 🟡 建议 | `:123-132` `extract_string` | 朴素子串搜索 + **不解转义**：`json_escape` 的输出被原样读回 ⇒ 值含 `\"`、`\\`、`\n` 时往返错乱。 |
| 🟡 建议 | `:12-26` `json_escape` | 只转义 5 个字符，**不转义 0x00–0x1F 控制字符** ⇒ 产出非法 JSON（`canonical_hash.cpp:127-141` 的同名函数做对了，`\u%04x`）。 |
| ✅ 通过 | `:70-93` `validate`、`:163-188` 的存在性检查 | 真实 fail-closed。 |

---

### 3.9 `src/memory_budget.cpp`（57 行）— **通过（1 条须修）**

**判定：全片最干净的文件。**

- ✅ `:50-52` 的溢出安全式经我独立复算**正确**：`resolve(105,95) == 99 == floor(105*95/100)`；`avail = 100q+r ⇒ q·pct + floor(r·pct/100) = floor(avail·pct/100)`。且 `pct ∈ [1,100]` 由 `runtime_resources_generated.h.in:18-19` 钉死 ⇒ `whole ≤ avail`，**无回绕**。注释「溢出安全」**属实**（与 `memory_pressure.cpp:54` 的过度声称不同）。
- ✅ `:37-42` 越界 ⇒ `INVALID_PERCENT` + `limit_bytes = 0`，**不静默 clamp**（符合 fail-closed）。
- 🟠 须修 `:43-47` + `runtime_resources.json` `available_memory_probe` 键 | **「fail-closed」标签贴在 fail-open 行为上**。`available_bytes == 0` ⇒ `limit_bytes = 0` + `source = NONE` ⇒ **回压不启用**，内存无上界。配置文件原文称其为「**fail-closed**：不启用回压」，而 `memory_pressure.cpp:298` 对**同一行为**的注释是「**fail-open** 到无治理」。**行为相同、标签相反的两处权威文本并存**，且配置那份是错的（不可判定时放行 = fail-open）。见 §4-B。
- 🟡 建议 `:35` | 注释硬写「（95）」紧挨取自生成头的值；JSON 一改即漂移（今日一致）。

---

### 3.10 `src/normalize_workflow.cpp`（503 行）— **阻断（部分未亲读，见 §1.4）**

| 判定 | 位置 | 问题 | 强度 |
|---|---|---|---|
| 🔴 阻断 | `:293` `frame.consume(blk, n.id);` | **返回值完全丢弃**（无赋值、无判断）。对照 `block_frame.cpp:191-206` 的 3 个失败返回点，其中「**未声明消费者**」的失败在此**完全不可见**：代码改用 `b1 < b0`（字节下降）反推销毁。⇒ 与 §3.3 同源的 fail-open。 | **双侧**（我亲读 `:291-306`） |
| 🔴 阻断 | `:244-249` | `frame.create(...)` 返回 `nullptr` 时**无 else、无 error、无探针**。而 `create` 有 4 条失败路径（重名 / 非法名 / `>kMaxBlockBytes` / 帧超限，见 `block_frame.cpp:151-155`）。⇒ 块没建成，节点照跑，`:342` `if (out.error.empty()) out.ok = true;` ⇒ **报成功**。 | **双侧** |
| 🔴 阻断 | `:261-265` | `catch (...) { ok = false; }` 吞掉全部异常身份，最终落到 `:319` `"node_failed:" + n.id` —— 小写、含冒号、非稳定错误码。`ERROR_HANDLING_STANDARD.md:78` 判红。 | **双侧** |
| 🟠 须修 | `:327` | 校验和只覆盖 **4 个硬编码块名** `{raw, calibrated, photometric, snr}`，而头文件自称「帧内**所有**块内容」。名单外块被篡改，1/N-worker 逐位一致门**照样绿**。 | 双侧 |
| 🟠 须修 | `:324` vs `:327` | 注释「按块名**升序**」不实：字面表序为 `raw, calibrated, photometric, snr`，字典序应为 `calibrated, photometric, raw, snr`。 | **双侧**（我亲读） |
| 🟠 须修 | `:452` | 全部帧一次性压入 `pending_`，`cfg_.queue_depth` **零引用** ⇒ 无界队列，违反 `CONCURRENCY_STANDARD.md:33`。 | 单侧 |
| 🟠 须修 | `:469-470` | 悬空引用 `docs/ACSD_DESIGN.md §8.3:643 / §8.3:647` —— `§8.3` 实为 `ACSD_DESIGN.md:469-481`，`:643/:647` 落在 §12.5。同一坏引用在 `runtime_resources.json:64` 复制扩散。 | 单侧 |
| 🟠 须修 | `:479-480` | `prefetch_threads` **无下界 clamp**：`requested = -1` ⇒ `reserve(static_cast<size_t>(-1))` ⇒ `std::length_error`。 | 单侧 |
| ✅ 通过 | `:464,:466,:488-491` | 池是 `run()` 内**局部量**，`run` 返回前全部 `join`，**无 `detach`**，头文件无 `std::thread` 成员 ⇒ **本文件无「私建线程池」违规**（子代理逐条核 `RT-004` 门亦确认）。 | 双侧 |

---

### 3.11 `src/mosaic_window.cpp`（369 行）— **阻断（部分未亲读，见 §1.4）**

| 判定 | 位置 | 问题 | 强度 |
|---|---|---|---|
| 🔴 阻断 | `:177` vs `:205` | **注释与代码直接相反**。`:177` 称「稠密 SNR 现场求值：逐像素即时算，**不预计算稠密面、不驻留**」；`:205` `out.pixel_values.push_back(v)` 把**每个像素物化进随 outcome 返回的缓冲**（`:238`）。头文件 `mosaic_window.h:12-13` 同声。⇒ 所谓「峰值驻留实测值」（`:179-186`）量的正是这条**设计声称不该存在的缓冲**。 | **双侧**（我亲读 `:170-238`） |
| 🔴 阻断 | `:187` + `:211-213` | **自洽式断言**：`peak_bytes` 主项 `out.pixel_values.capacity()*sizeof(double)` 与判据上界共用同一 `kPixelsPerTile` 容量模型、且带 2× 余量 ⇒ 改错容量口径三条驻留门都不会红。子代理进一步构造反例：若按设计改成**流式输出（更正确的实现）**，`peak` 变常数 ⇒ 单调性门**判红** ⇒ **当前门在给违规实现发绿、对正确实现判红，方向是反的**。 | 双侧（结论），单侧（E3 算例） |
| 🔴 阻断 | `:52` | 零覆盖像素返回 **`0.0` 哨兵**。`NUMERIC_STANDARD.md:101-103` 明写「无效的唯一表示 = **NaN**（`0`、`±Inf` 或任意哨兵值都不构成无效表示）」。 | 单侧 |
| 🟠 须修 | `:278` | **成功路径无条件 `cancel()`**，且 `run()` `:260-289` **从不复位 `cancel_`** ⇒ 第二次 `run()` 全窗返回 `error="cancelled"`、`pixels=0`。对象事实上单次可用。 | **双侧**（我亲读 `:260-289`） |
| 🟠 须修 | `:263-267` | 只复位 `next_`/`completed_`，**不复位** `total_read_`/`naive_baseline_`/`peak_inflight_` ⇒ 二次运行计数翻倍。 | 双侧 |
| 🟠 须修 | `:188` | 第 4 处误植 FNV basis；且 `:206-208` 直接哈希 `double` 的**原始字节** ⇒ 校验和**依赖字节序与 IEEE 位型**，跨平台（x86/ARM）不一致，与 G08-09 双平台目标冲突。 | **双侧**（我亲读 `:188,:206-208`） |
| 🟠 须修 | `:105,:132` | `make_dirs` / `write_file_atomic` 返回值**双双丢弃**；该 manifest 承载 `SCHEDULER_CONTRACT.md:36` 的「每阶段必填」资源声明 ⇒ 写失败零信号。 | 单侧 |
| 🟡 建议 | `:191-192` vs `:184-186` | `resident_fixed` 取 `relevant.capacity()*8`（覆盖任一 tile 的**全部**帧），注释却称「不含 `tile_bytes`…本进程不物化、不驻留」。 | 单侧 |
| ✅ 通过 | `:268-273` | 池是本次 `run` 的有界作用域量，返回前全部 `join`，**无 `detach`** ⇒ **无「私建线程池」违规**。 | 双侧 |

---

### 3.12 `core/phase_lifecycle.README.md`（70 行）— **阻断（悬空引用密集）**

**这是本片最严重的文档缺陷：一个把「验收映射」写成表格、却指向不存在的测试的权威文档。**

| 判定 | 位置 | 问题 |
|---|---|---|
| 🔴 阻断 | `:52-54` §3 验收映射表 | 表中 5 条验收有 **3 条指向不存在的测试文件**（本人 `ls` 实证）：<br>• `test_global_state_spy.py` — **MISSING**<br>• `test_phase3_external_fixture_independent.py` — **MISSING**<br>• `test_no_cross_phase_graph.py` — **MISSING**<br>`eng/tests/runtime/` 实际只有 `__init__.py`、`integration/`、`test_phase_lifecycle.py`、`test_rt005_plan_estimator.py`、`test_typed_dag_negative.py`。⇒ 「进程内全局状态 spy」「phase1 DLL 缺失不影响 phase3」「禁止 `--phases` 调用图」三项**验收证据不存在**。 |
| 🔴 阻断 | `:13-16` 约束来源 | 三个上游依据**全部不存在**：`ACSD_ENGINEERING_CONSTRAINTS.md`（MISSING，旧名）、`03_TARGET_PRODUCT_AND_ARCHITECTURE.md` §5（MISSING）、`tasks/03_RUNTIME_DATA_IO_TASKS.md`（MISSING）。 |
| 🟠 须修 | `:67-70` §5 一致性自检 | 「`python3 phase_lifecycle.py --registry-isolation-check` → `PHASE_REGISTRY_ISOLATION PASS`」——**这是负责人明令不得采信的「机器读通过」**。本人复核 `phase_lifecycle.py:224-235`：它只统计 `assert_registry_view_isolated()` 的返回泄漏数，**不触碰全局单例、Store、RunContext**这三项 `:52` 声称的核心验收。**该 PASS 不构成 §3 表中任一验收的证据**。 |
| 🟠 须修 | `:3-5` | `owner: SA-RT-05`（人员/任务流水号）+「三形态必须同步修改」——违反 AGENTS.md §5「正文无…任务流水编号」。 |
| 🟠 须修 | `:4` | 声称「机器一致性由 `eng/tests/runtime/test_phase_lifecycle.py` 校验」——该文件**存在**，但 §3 表中 3 条验收的测试不存在 ⇒ 「机器一致性」覆盖范围远小于声称。 |
| 🟡 建议 | 全文 | 文件自称「设计权威」却位于 `lib/`（生产源码）内，绕过 `docs/` 权威链；且 AGENTS.md §5「文档只写现行设计」——`:63-65`「本任务不改…」「本任务交付…」是任务叙事。 |

---

### 3.13 `README.md`（35 行）— **须修**

| 判定 | 位置 | 问题 |
|---|---|---|
| 🟠 须修 | `:21` | **悬空行号**：`ipv_select.cpp:57 IPV_ARCSEC_PER_UM_PER_MM=206.265`。实测该常量在**第 58 行**，第 57 行是注释 `// 206.265 = (180×3600)/π...`。 |
| 🟠 须修 | `:20-22`（引出的上游） | 该 README 锚定的常量，其**注释所声称的恒等式是假的**：`ipv_select.cpp:57` 写 `206.265 = (180×3600)/π`（漏掉 `×1e-3`），`docs/science/DATA_SEMANTICS.md:1074` 写 `206.265 = (180×3600)/π × 1e-3`。本人 `python3` 复算：`(180*3600)/π × 1e-3 = 206.26480624709637`，与 `206.265` 差 **0.94 ppm**。⇒ **正本与源码各写一个假的精确恒等式**。 |
| 🟡 建议 | `:30` | 「旧值 `header_crval` 已移除」= 历史叙事，违反 AGENTS.md §5「无『旧版/作废/曾/原』等历史叙事」。 |
| ✅ 通过 | `:13` `docs/science/DATA_SEMANTICS.md` §18.5 | **实测存在**（`:1051` 「### 18.5 初始指向来源（wcs.init_source）」）。这是本片唯一完全准确的外部引用。 |
| ✅ 通过 | `:5` `src/module_adapters.cpp`、`:4` `core/phase_lifecycle.py`、`:35` `eng/tests/unit/` | 均实测存在。 |

---

## 4. 发现清单

### 4.1 阻断（9 条）

| ID | 问题 | 位置 |
|---|---|---|
| **BL-1** | FITS 数据单元字节数计算**有符号整数溢出 UB**，防溢出守卫位于溢出之后 | `canonical_hash.cpp:310,313,315`（守卫 `:317`） |
| **BL-2** | `naxis.assign(n,0)` 无上界 ⇒ 畸形 FITS 触发 `bad_alloc`，从非异常契约函数逃逸 | `canonical_hash.cpp:269` |
| **BL-3** | `replay_same_run` **旁路** `commit_node` 的「半成品拒绝」，可把已提交节点的产物清空并返回成功 | `checkpoint.cpp:82-88` vs `:44-47` |
| **BL-4** | `consume()` 3 个失败原因被丢弃/混同 ⇒ **未声明消费者**违规整轮报 `ok=true` | `block_flow.cpp:213-214`；`normalize_workflow.cpp:293` |
| **BL-5** | `frame.create()` 返回 `nullptr` **无 else** ⇒ 块未建成仍报成功 | `normalize_workflow.cpp:244-249` + `:342` |
| **BL-6** | 恒真门，且**打印标签声称验证了它并未验证的事** | `budget.py:565`（`:429` 保证） |
| **BL-7** | `PressurePolicy` 零阈值**默认构造即活锁**（永久 HIGH + 每采样丢帧），头文件承诺的兜底在 `.cpp` 中不存在 | 头 `memory_pressure.h:108-114` + `memory_pressure.cpp:334,384-385` |
| **BL-8** | 台账落盘失败被 `(void)` 丢弃，**且失败时缓冲不清** ⇒ 管内存的组件自身无界泄漏 + 证据面全丢 | `memory_pressure.cpp:168,174,210-214` |
| **BL-9** | §3 验收映射表中 **3/5 条验收指向不存在的测试**；3 个上游约束文档全部不存在；§5 的 `PHASE_REGISTRY_ISOLATION PASS` 不构成该表任一验收的证据 | `core/phase_lifecycle.README.md:13-16,52-54,67-70` |
| **BL-10** | 注释「不预计算稠密面、不驻留」与 `push_back` 直接相反；`peak_bytes` 量的是设计声称不存在的缓冲 ⇒ **驻留门方向是反的（给违规实现发绿）** | `mosaic_window.cpp:177` vs `:205`；`:187,211-213` |
| **BL-11** | **`budget.py` 的 `ProcessBudgetRegistry` 是 C++ 真本的无锁、吞错第二份实现，5 处语义已漂移** | `budget.py:108-171` vs `lib/infrastructure/cli/runtime_contract.h:208-288` |

### 4.1b BL-11 详述（本片新发现的「两份实现」病根）

`budget.py:4-5` 的模块 docstring 逐字声称：「宪章 §10.4：一个进程只有一个资源调度器与线程预算源；模块按 work unit 申请线程租约，**Runtime 防止嵌套并行和超额订阅**」。但该类：

| 语义 | C++ 真本 `runtime_contract.h` | Python `budget.py` |
|---|---|---|
| 并发保护 | **每个方法**都 `lock_guard(mu_)`（9 处） | **完全无锁**：`request_lease` 的 `avail = cores - _leased`（`:154`）与 `_leased += ask`（`:161`）是**非原子读-改-写** ⇒ 两线程可同时读到同一 `avail` 并双双获批 |
| 无 source 申请 | `*err = "no budget source registered (violates §10.4)"`（`:250`） | 静默 `return 0`（`:152-153`），**不计入 `_denied`** |
| `want == 0` | `*err = "empty lease request"`（`:256`） | 静默 `return 0`（`:156-157`） |
| 超额订阅 | `*err = "lease exceeds available budget…"`（`:261`） | `return 0` + 计数，**无错误**（`:159-160`） |
| `release_lease` | `(n >= leased) ? 0 : leased - n`（`:269`，`uint32_t` ⇒ 饱和、负值不可能） | `max(0, _leased - int(n))`（`:165`，**有符号** ⇒ 负 `n` **放大**租约） |
| 生产调用者 | `runtime_contract_test.cpp`、`mode_gate.h:56-57` | **零**（仅自身 selftest `:538-547`） |

⇒ 三重问题：① **无锁**使其在最需要保护的维度（防超额订阅）上判别力为零；② **零生产调用者**，守门能力实际为零；③ 与真本**已实际漂移**（见 §5 CE-20）。`budget.py:13` 自称「不同实现，**非自证**」——对**采样**成立，但 `ProcessBudgetRegistry` 是**控制面**（发放与拒绝租约），把控制面复制一份正是 `runtime_contract.h:52-53` 自己点名要治的病。

**唯一验它的 selftest 是教科书式自洽式断言**：`budget.py:543-546` 用该类**自身返回值**作期望、在**单线程**下运行 ⇒ 在「缺锁」这一唯一真实缺陷所在的维度上**判别力为零**。

### 4.2 须修（14 条，摘要）

`prune_json` 任意深度按键名剪枝致产品哈希碰撞（`canonical_hash.cpp:192-214`）｜`snprintf` 静默截断 ≥100 GB（`:343-346`）｜FITS 检出口径大小写不一致（`:386,413`）｜`commit_node` 不查重致 `committed_nodes()` 返回重复（`checkpoint.cpp:53-55,70-76`）｜`set_input` 的 `element_count` 零校验（`block_flow.cpp:87-93`）｜`parse_stage_block_flow_spec` 异常逃逸（`:287-318`）｜`product()` 跨单元被清空与注释相反（`:243,245-246`）｜`request_lease` 把「要 0 个」当「拿全部」（`budget.py:151-162`）｜`release_lease` 可重复释放他人租约（`:164-166`）｜`rss_growth` 只取末采样瞬时斜率（`:475,503`）｜`io_wait_percent` 全机量当进程量（`:374-384,470`）｜零采样仍退出 0（`:454-455,625,636`）｜`execution_class` 不校验闭集 + ACR 前缀可绕 + `export_index_json` 丢 `ports` 且恒 true（`module.cpp:53,58,156-177`）｜台账 `json_escape` 不转义控制字符（`memory_pressure.cpp:73-82`）

### 4.2b 单侧采纳的子代理补充（证据强度：子代理单侧，本人未亲跑 grep / 未亲读该行）

| ID | 问题 | 位置 |
|---|---|---|
| **M1** | **配置声明的两个台账事件全仓不存在**：`runtime_resources.json` 的 `memory_pressure_ledger` 键列出 `governance_disabled` / `dispatch_escape_no_inflight`，全仓 grep **只在该行 JSON 命中，代码零命中**（关闭态 `:290-297` 走「零副作用」提前返回，永不登记）。反向缺口：代码实际发的 `evict_no_candidate`（`:389`）配置里没登记。 | `memory_pressure.cpp:290-297,389` + `runtime_resources.json` |
| **M3** | **压力台账与冻结的探针事件 schema 不兼容且无任何校验器**：`SCHEDULER_CONTRACT.md` §4 冻结 schema（`required=[ts,stage,kind,value,unit]`；`kind` 枚举 8 值；**`frame_id` 为 integer**），§6 要求「探针缺 `stage`/`kind` ⇒ 判红」，§4:58 断言「事件格式只有这一份」。实际台账（`:189-208`）**缺 4/5 必填字段**，且 `frame_id` 写**字符串**（`:201`）而 schema 要求 integer。eng/ grep `pressure_ledger` 只有那一行 JSON，**无校验器、无 `$id`、未登记进 `unified_object_registry.json`**。⇒ 两条分支都违约。 | `memory_pressure.cpp:189-208` |
| **M20** | **`high_percent=90` 被无条件用于其推导域之外**。按 `runtime_resources.json` 自己的 `memory_pressure_high_percent_derivation`（`100 − high ≥ 100×1.6724/B`，B = 0.95×可用内存）代入复算：<br>· A=23.3 GB ⇒ 要求 high ≤ 92.4，一帧需 7.6 pp，固定 90 留 10 pp ✓<br>· A=17.6 GB ⇒ 要求 ≤ 90.0，一帧需 10.0 pp，留 10 pp ✓（临界）<br>· **A=8.0 GB ⇒ 要求 ≤ 78.0，一帧需 22.0 pp，只留 10 pp ✗**<br>· **A=4.0 GB ⇒ 要求 ≤ 56.0，一帧需 44.0 pp，只留 10 pp ✗**<br>`memory_pressure_derivation_scope` 声明了适用域，但**无任何代码读取或强制**；`.h.in` 对这 7 个键**无 `static_assert`**（对比 `:44-48,57-60,67-68` 对 p3/queue/cpu_budget 都守了）。⇒ ≤8 GB 宿主上，越过高水位仍放行新帧，而该帧边际 RSS 是剩余余量的 2–4 倍。 | `runtime_resources.json` + `runtime_resources_generated.h.in` |
| **M7** | **`budget.py:585-588` 的 Windows 降级断言在其目标平台上必然失败**：`_IS_WINDOWS=True` 且 `_win_read_status` 已还原为**真实实现**时求值。Linux 上 `ctypes.windll` 抛 `AttributeError` ⇒ 返回 0 ⇒ 通过；**真 Windows 上返回真实值 ⇒ 断言失败**。`ci_windows_driver` 已随 G08-01 删除 ⇒ 无人跑、也跑不通。 | `budget.py:583-588` |
| **S3** | **`memory_budget.cpp:51` 的 `rem` 项无精确 oracle，且存在存活变异**：`mem_wire_test.cpp:421/425` 的 `available_bytes` **全是 100 的倍数**（`rem ≡ 0`）；唯一非倍数用例（`:435` `~0ull`）只断言区间。**已复算存活变异**：删掉 `:51` 的 `/100u` ⇒ `~0ull` 上得 `17524406870024075445`（正确值 `…034`），仍在断言区间内 ⇒ **不被捕获**。（公式本身正确。） | `memory_budget.cpp:51` + `mem_wire_test.cpp` |
| **M4** | **裸 `1e-6` epsilon 违反正本 MUST**：`budget.py:409` `dt = max(1e-6, …)`、`:469` `span = max(1e-6, …)`。`NUMERIC_STANDARD.md:115` 逐字：「epsilon 必须说明物理/数值来源；**裸 `1e-6` 视为无来源**」。 | `budget.py:409,469` |
| **M10** | **台账把配置错误记成探针失败**：`available()` 只看 `limit_bytes > 0`；当比例越界导致 `INVALID_PERCENT` 时 `limit_bytes` 也是 0，于是走 `:308` 第一个分支，写下 `"budget undecidable (available memory probe returned 0 or percent invalid)"`。而 `budget.source`（`:616` 已暴露）此时明明是 `invalid_percent`。⇒ **配置错误被记成探针失败，误导全部事后归因**。 | `memory_pressure.cpp:308-311` vs `:616` |
| **S12** | **`budget.py:167-171` `reset_for_test` 是生产可达的公开 API**：任何调用者都能清空 §10.4 的单一预算源后重新登记，无 `_for_test` 隔离。 | `budget.py:167-171` |

### 4.2c 悬空引用（子代理 grep 实证，单侧；与 §3.12 本人亲验的 7 条互补）

1. **`eng/tools/quality/runtime_oracle.py` 已不存在**（只剩 `__pycache__/runtime_oracle.cpython-313.pyc`），但 `docs/engineering/EXECUTION_MODEL.md:140` 仍称 budget.py 由它「锚定 ⇒ **在役 CI 面**」。
2. **`check_budget_single_source.py` 已不存在**（只剩 `.pyc`），但 `runtime_resources.json` 的 `frame_memory_gate/note` 仍称「由 **CHK-BUDGET-SINGLE-SOURCE** 与实现侧字面量逐位比对」⇒ **本片预算常量现已无任何活体单源门**。
3. **`eng/tools/quality/__pycache__/` 下 42 个 `.pyc` 无对应源码**，含 `check_taut_null_guard.py`、`check_test_discriminative.py`、`check_conclusion_truth.py` —— **恰是「恒真/空断言」与「判据判别力」类门**。`__pycache__` 内的 `.pyc` 不是可导入的 sourceless 模块 ⇒ 这些门已实质死亡。
4. 删除来源可追溯：commit `e5f589a6`「G08-01 物理删除旧门禁与 CI（344 件 / −136676 行）」。**删除是有意治理动作，问题在于文档未同步**（AGENTS.md §3）。

> ⚠️ **对上游的重要提示**：第 3 条意味着——发现 BL-6（`budget.py:565` 恒真门）与 M7 这类**恒真门**的检查器（`check_taut_null_guard.py` / `check_test_discriminative.py`）已随 G08-01 一并删除且未重建。这与负责人「正确性只靠对抗性审核得出」的裁决方向一致，但**该缺口应在 `UNRESOLVED_REGISTER` 登记**。

### 4.3 建议（10 条，摘要）

`pipeline.cpp:119-120` 异常逃逸｜`pipeline.cpp:156` `parallel` 静默默认 false｜`pipeline.cpp:34-53` `to_json` 丢 `config_ref`（sha256 绑定消失）｜`artifact.cpp:95-119`↔`:163-188` 往返丢 provenance/coordinate/ownership｜`artifact.cpp:12-26` 控制字符未转义｜`memory_pressure.cpp:100` `frame_evictions` 无界｜`memory_pressure.cpp:55` `kPressurePercentSaturated` 悬空符号｜`memory_pressure.cpp:373-375` no-op 死分支｜`budget.py:495-507` 5 个阈值硬编码｜`README.md:30` 历史叙事

### 4.4 本片**未**发现的问题（据实登记，避免过度指控）

- **私建线程池**：本片 **0 处**。`normalize_workflow.cpp:464,466,488-491` 与 `mosaic_window.cpp:268-273` 的池均为 `run()` 内**局部量**、返回前全部 `join`、**无 `detach`**、头文件无 `std::thread` 成员；`memory_pressure.cpp:640` 是 `thread_local` + RAII `GovernorScope`，**非**进程全局单例。⇒ **不属已知 9 处违规**（子代理逐条 grep 复核同侧）。
- **退役对象仍有活调用者**：本片未发现「自称零消费者但有活调用者」的实例。**反向**：`choose_victim` 是**自称生产语义、实际生产零调用者**（与既往失效模式相反）。
- **`memory_budget.cpp:50-52` 溢出安全**：经独立复算**属实**，不构成缺陷。
- **`tail[96]`**：经复算为**静默截断**（≥100 GB），**不是**缓冲区溢出 —— 差一点误判，特此记录以免下游复现错误结论。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| **CE-1** | `checkpoint.cpp`：`begin("r1")`→`commit_node("n1",{"a1"})`→`replay_same_run("n1",{})` | 「半成品拒绝」不变量 | **推翻** ✅ 命中 `:82-88` 旁路 |
| **CE-2** | `block_flow.cpp`：块 x 的 `consumers=["n3"]`，节点 n2 声明 `reads=["x"]` | 「未声明读 = fail-closed」（`:144`） | **推翻** ✅ `consume` 在 `block_frame.cpp:197` 返回 false，代码当「未销毁」，`:270` 报 `ok=true` |
| **CE-3** | `normalize_workflow.cpp:244`：`frame.create` 返回 nullptr（4 条失败路径任一） | 无 else 分支是否安全 | **推翻** ✅ 无 else，`:342` 仍可置 `ok=true` |
| **CE-4** | `budget.py:429`⇒`:565`：`max(0.0,dsum)` 后判 `>= 0.0`；Windows 下 `tcpu={}` | 「per-thread cpu delta sampled」有判别力 | **推翻** ✅ 恒真，零采样仍 `[ok]` |
| **CE-5** | `budget.py`：`cores=8, leased=0, request_lease("A", 0)` | 租约按需分配 | **推翻** ✅ 授予 8（全池），且不计入 `_denied` |
| **CE-6** | `budget.py:475`：前 9 采样 100 MB/s 泄漏、第 10 采样持平 | `>32 MB/s` 能抓泄漏 | **推翻** ✅ 只取末瞬时斜率 ⇒ 门全绿 |
| **CE-7** | `memory_pressure.h` 默认构造 `PressurePolicy{}` 后跑 `step_locked` | 「0 = 未配置 ⇒ 由配置层填默认」 | **推翻** ✅ 首采样即 HIGH+STOP_DISPATCH，`th=max(1,0)=1` 致每采样丢帧 |
| **CE-8** | `module.cpp`：`execution_class="CPU_HEAVY", parallel_ok=false` | 「heavy+serial 拒绝」 | **推翻** ✅ `:53` 裸串比不触发 |
| **CE-9** | `module.cpp`：`module_id="acsd.acr"`（无尾点） | 「ACR 不接生产」 | **推翻** ✅ `"acsd.acr."` 前缀不匹配 |
| **CE-10** | `module.cpp`：`export_index_json(nullptr)` | bool 返回反映成功 | **推翻** ✅ 构造完整文档后丢弃仍 return true |
| **CE-11** | `canonical_hash.cpp:269`：`NAXIS = 1000000000000` | 结构非法应被 `return false` | **推翻** ✅ 先 `assign` 8 TB 容器，`bad_alloc` 逃逸 |
| **CE-12** | `canonical_hash.cpp:313`：`NAXIS1=2^40, NAXIS2=2^40` | `:317` 的 `nbytes<0` 能拦住 | **推翻** ✅ 守卫在 UB 之后，无效 |
| **CE-13** | `canonical_hash.cpp:192-214`：`{"stars":[{"sha256":"aaa"}]}` vs `…bbb…` | 产品哈希应区分不同产品 | **推翻** ✅ 剪枝后同为 `{"stars":[{}]}` ⇒ **哈希相同** |
| **CE-14** | `artifact.cpp`：round-trip 一个 descriptor | provenance 应保留 | **推翻** ✅ `coordinate`/`ownership`/`provenance` 全部静默丢失 |
| **CE-15** | `python3` 复算 `1469598103934665603` | 「FNV-1a 64」 | **推翻** ✅ `std//10 == code` 为 True，掉了末位 `7`（但全仓 10 处一致 ⇒ 统一约定，非局部笔误） |
| **CE-16** | `python3` 复算 `(180*3600)/π × 1e-3` | `206.265` 的精确恒等式 | **推翻** ✅ 真值 `206.26480624709637`，差 **0.94 ppm** |
| **CE-17** | `python3` 复算 `tail[96]` 预算 | 是否缓冲区溢出 | **未推翻**（我自己的假设被推翻）⇒ 是**静默截断**，阈值 **≥10¹¹ B = 100 GB**。**如实记录该自我更正** |
| **CE-18** | `ls eng/tests/runtime/` | §3 验收映射表 5 条测试存在 | **推翻** ✅ 3 条 MISSING |
| **CE-19** | 读 `phase_lifecycle.py:224-235` | `PHASE_REGISTRY_ISOLATION PASS` 覆盖 §3 验收 | **推翻** ✅ 只数 registry 视图泄漏，不触碰全局单例/Store/RunContext |
| **CE-20** | `python3` 复算 `release_lease(-5)`：Python vs C++ 真本 | 两份 `ProcessBudgetRegistry` 语义相同 | **推翻** ✅（本人复算）`budget.py:165` `max(0, 3-(-5)) = 8` ⇒ `avail = max(0, 8-8) = 0` ⇒ **此后任何 `request_lease` 恒返回 0，预算永久锁死**；C++ `uint32_t` 版 `n` 饱和后走 `n >= leased ⇒ 0` ⇒ 归 0。**同输入，两份实现结论相反。**<br>⚠️ **对子代理的更正**：其称「`_leased(8) > _cores(8)` 越界态无人报警」——**不成立**（8 = 8，未越界）；但「预算锁死」结论**仍成立**（`avail` 归 0 使后续一切申请失败）。 |
| **CE-21** | `memory_pressure.cpp:73-82` `json_escape` + `json.loads`（子代理实测） | 台账行可被解析 | **推翻** ✅ `set_frame_label(t, "obs\t2024")` 产出含**裸 TAB** 的 JSONL 行，`json.loads` 抛 `Invalid control character`。而 `module_adapters.cpp:1918/1926` 的 label 取自**输入路径**，POSIX 文件名合法含 TAB ⇒ **合法输入即产出不可解析台账**。 |
| **CE-22** | `choose_victim` vs `evict_one_locked` 同状态选帧（子代理构造） | 两份淘汰实现语义相同 | **推翻** ✅ 取 `f1(ev=0,ticket=1)`、`f2(ev=1,ticket=2)` 同 `stage_index`：`evict_one_locked` 选 **f1**（丢弃次数升序主键），`choose_victim` 选 **f2**（ticket 降序）⇒ **选出相反的帧**。 |
| **CE-23** | 关闭态计数残留（子代理构造） | 「零计数、逐字节等价」 | **推翻** ✅ `:290-297` 不清任何计数器 ⇒ `set_policy(enabled=false)` 后 `status_json` 仍输出 `"stop_events":1`；再置回 `true` 时 `high_streak` 残留 ⇒ **单次采样即进 HIGH，绕过去抖**。 |
| **CE-24** | `module.cpp` 注册面 vs 实际实例（子代理构造） | `export_index_json` 发布的面与可执行体一致 | **推翻** ✅ 工厂返回的实例可携带 `version="2.0"`/`execution_class="cpu_heavy"`，而 `:117` 只比 `module_id` ⇒ 索引发布 `"version":"1.0","execution_class":"io_bound","executable":true`，**三者互不相符且无一报错**。 |

### 5.1 被自己的反例推翻的假设（如实登记，防下游误用）

| 我原本的怀疑 | 反例结果 | 处置 |
|---|---|---|
| `tail[96]` 是缓冲区溢出 | `snprintf` 不溢出，只截断 | **降级**为「静默截断，≥100 GB」 |
| `pressure_percent_of` 的 `whole*100` 回绕在生产可达 | 需 rss ≥ 184.5 PB | **降级**为建议（子代理同侧独立得出） |
| `block_flow.cpp:265` `fnv1a(h, b->raw(), b->bytes())` 空指针解引用 | `bytes()==0` ⇒ `n==0` ⇒ 循环不解引用 | **撤销**，不作为缺陷上报 |
| `memory_budget.cpp:50` `whole=(avail/100)*pct` 溢出 | `pct ≤ 100` 由 `.h.in:18-19` 钉死 ⇒ `whole ≤ avail` | **撤销**，「溢出安全」属实 |
| UTF-8 字节序 ≠ Python code point 序（`canonical_hash.cpp:175`） | UTF-8 的设计性质即字节序 = 码点序 | **撤销**，两侧一致 |
| `g_current_governor` 是违规进程全局单例 | `thread_local` + RAII 作用域保存/恢复 | **撤销**，不违反 RT-002 |

---

## 6. 盲复算

**方法**：遮住既有判定与子代理结论，仅凭原文 + 正本独立取证，再回比。口径三档：**一致 / 偏松 / 偏严**（指「自称的保证」相对「机制实际交付」）。

| 自称 | 位置 | 独立取证结果 | 判定 |
|---|---|---|---|
| 「半成品拒绝」 | `checkpoint.cpp:43-47` | `replay_same_run` 旁路 | **偏松** |
| 「未声明读 = fail-closed」 | `block_flow.cpp:144` | 只对缺块成立，未声明消费者不成立 | **偏松** |
| 「按块名升序」 | `normalize_workflow.cpp:324` | 字面表序非字典序 | **偏松（注释不实）** |
| 「帧内**所有**块内容的确定性校验和」 | `normalize_workflow.h:154` | 只哈希 4 个硬编码名 | **偏松** |
| 「FNV-1a 64 稳定 hash」 | `artifact.cpp:49` | basis 缺末位 `7` | **偏松** |
| 「不预计算稠密面、不驻留」 | `mosaic_window.cpp:177` | 全量 `push_back` 并随 outcome 返回 | **偏松（相反）** |
| 「④b 峰值驻留 = 实测值」 | `mosaic_window.cpp:179-186` | 主项是设计声称不该存在的缓冲 | **偏松** |
| 「0 = 未配置 ⇒ 由配置层填默认」 | `memory_pressure.h:108-114` | `.cpp` 无兜底 | **偏松** |
| 「候选 = 未越过安全点、**未 ended**、未被选过」 | `memory_pressure.cpp:130` | `ended` 无写入点；`choose_victim` 不查 | **偏松** |
| 「溢出安全、语义恒为 floor」 | `memory_budget.cpp:48-49` | 复算 `resolve(105,95)==99` 成立 | **一致** ✅ |
| 「与 Python 逐字节同构」 | `canonical_hash.cpp:3-6` | 排除键、编码、UTF-8 序均对齐；未逐项复核 Python 镜像 | **一致（未完全取证）** |
| 「平台分派必须真的切到 Windows 面…不静默假装成功」 | `budget.py:567-571` | 负例在 Linux 上确有判别力 | **一致** ✅ |
| 「失败时缓冲**原样保留**（不丢证据）」 | `memory_pressure.cpp:159` | 缓冲确实保留，但错误被 `(void)` 丢弃 ⇒ 保留的缓冲**再也没人知道写失败了** | **偏松** |
| 「fail-closed：不启用回压」 | `runtime_resources.json` `available_memory_probe` | 行为是 fail-open（不可判定时放行） | **偏松（标签错误）** |

**盲复算总判：既有结论/代码注释整体「偏松」。** 未发现「实现比声称更严」的情形；少数做对的地方（`memory_budget.cpp` 溢出安全、UTF-8 键序、`budget.py` Windows 负例、`GovernorScope` 非单例）已逐条确认为**一致**，不虚报缺陷。

---

## 7. 子代理派发记录

**派发 5 个**（纪律要求 3–5）。

| # | 分片 | 状态 | 覆盖 |
|---|---|---|---|
| S1 | `memory_pressure.cpp` + `memory_budget.cpp` + `budget.py` + `module.cpp`（1533 行） | ✅ 已交付 | 4/4 全文 |
| S2 | `canonical_hash.cpp` + `artifact.cpp` + `checkpoint.cpp`（840 行） | 未在本轮交付前收回 | — |
| S3 | `normalize_workflow.cpp` + `mosaic_window.cpp`（872 行） | ✅ 已交付 | 2/2 全文 |
| S4 | `pipeline.cpp` + `block_flow.cpp` + 两份 README + `core/` 目录核验（781 行 + 目录） | 未在本轮交付前收回 | — |
| S5 | **与 S1 同分片**（首次派发时误发重复 prompt） | ✅ 已交付 | 4/4 全文 |

**最终状态**：5 个中 **3 个已交付**（S1、S3、S5），**2 个未交付**（S2、S4）。S1 与 S5 同分片但**各自独立取证**，结论高度一致（`budget.py:565` 恒真门、`request_lease` 哨兵、`module.cpp` 两处可绕、`choose_victim` 分叉），构成有效交叉印证。

**派发事故（如实登记）**：S5 与 S1 内容完全相同系**我的派发失误**（重复粘贴 prompt，非有意设计为交叉验证）。未取消成功（subagent 无对应 job 可 kill），遂**保留并作为 S1 的独立盲复算第二臂**——两次独立取证结论一致（`budget.py:565` 恒真门、`request_lease` 哨兵、`module.cpp` 两处可绕），构成有效交叉印证。

### 7.1 逐条复核：子代理主张 → 本人处置

| 子代理主张 | 我的复核 | 处置 |
|---|---|---|
| S1：`PressurePolicy` 零阈值 ⇒ 活锁（阻断） | **亲读** `memory_pressure.h:108-114` 确认 7 阈值全 0 + 注释「由配置层填默认」；亲读 `:334/:384-385` 确认恒真链 | **采纳并升为 BL-7** |
| S1：`budget.py:565` 恒真门 | **亲读** `:565` 与 `:429` 复算 | **采纳并升为 BL-6**，并**加强**：指出其 docstring `:346-350` 自认「改松以保持绿」，而 PASS 输出仍打 `[ok] … sampled` |
| S1：`request_lease` `want=0` ⇒ 授予全部 | **亲读** `:151-162` 构造算例 | **采纳** |
| S1：`rss_growth` 只取末瞬时斜率 | **亲读** `:475`+`:503`+`:434` | **采纳**（我补 `:434` 瞬时值定义这一环） |
| S1：`io_wait_percent` 全机量当进程量 | **亲读** `:374-384`+`:470` | **采纳** |
| S1：`execution_class` 不校验闭集 / `acsd.acr` 绕禁令 | **亲读** `module.cpp:14-83,53,58` | **采纳** |
| S1：`export_index_json` 丢 `ports` / 恒 true | **亲读** `:156-177` | **采纳** |
| S1：`choose_victim` 生产零调用者 + 语义分叉 | 接受（我亲读 `:131-156` vs `:510-545` 确认分叉与 `ended` 不查；**零调用者为单侧**，标注保留） | **采纳，零调用者一栏标单侧** |
| S1：`pressure_percent_of` 回绕 | 亲读确认 `whole*100` 无守卫 | **采纳为建议**（我补「需 184.5 PB，物理不可达」的量级限定） |
| S3：E3 反向不变式（驻留门惩罚流式实现） | **亲读** `:177` vs `:205` 确认矛盾；E3 算例为**单侧** | **矛盾部分双侧采纳**；**E3 算例标注单侧** |
| S3：`mosaic_window.cpp:52` 用 `0.0` 哨兵 | 未亲读该行 | **单侧采纳**，标注证据强度 |
| S3：`peak_inflight` 门恒真（`normalize_workflow_test.cpp:377`） | 该测试文件不在本片 | **超出本片**，登记为跨片线索，不计入本片判定 |
| S3：`LEDGER.md:97` 「源码级注入」与实际不符 | 超出本片 | **登记为跨片线索** |
| S3：两份审稿规范原件「未能定位」 | **本人已成功读取** `run/GOVERN-08/工作包-GOVERN-08原件/standards/04_对抗性审稿规范.md`（58 行） | **否决其「未能定位」自述**；并据此把 `:26`「恒真断言判无效」作为 BL-6 的权威依据 |
| S3：「两 TU 无生产调用者」 | 未亲跑 grep 全仓核对 | **单侧**，标注 |

### 7.2 被否决 / 降级的子代理主张

1. **否决** S3「两份审稿规范原件未能定位」——文件存在且本人已读。
2. **降级为单侧** S3 的 E3 算例、`:52` 哨兵、`:105/:132` 落盘丢弃、「两 TU 无生产调用者」——均在本人未亲读的行段。
3. **降级为建议** S1 的 `pressure_percent_of` 回绕——量级不可达。
4. **不计入本片** S3 的 `normalize_workflow_test.cpp` / `LEDGER.md` 两条——文件不属本片 13 份。
5. **记为派发失误** S5 与 S1 重复。

### 7.3 本片覆盖缺口（须登记）

S2（`canonical_hash.cpp`/`artifact.cpp`/`checkpoint.cpp`）与 S4（`pipeline.cpp`/`block_flow.cpp`/README/`core/` 目录核验）**在本轮交付前未收回**。本人已亲自逐行读完该 5 份（C5–C10 全部双侧取证），故**不影响本片判定**；但两份代理的独立交叉印证缺失，如实登记。

---

## 8. 自证段（可复跑命令）

> 全部为**只读**命令。不编译、不跑测试、不跑仓内二进制。

```bash
cd "/workspace/Astro CS Database"

# ── 0. 基线（本片报告称 HEAD=f4a2cf21，与交付要求写的 850a9ede 不符）
git -c core.quotepath=false log -1 --oneline

# ── 1. 覆盖率：13 份成员实际行数（合计应 = 4026）
for f in src/memory_pressure.cpp budget.py src/canonical_hash.cpp \
         src/normalize_workflow.cpp src/mosaic_window.cpp src/pipeline.cpp \
         src/block_flow.cpp src/artifact.cpp src/module.cpp \
         src/checkpoint.cpp core/phase_lifecycle.README.md \
         src/memory_budget.cpp README.md; do
  printf "%6s  %s\n" "$(wc -l < "lib/infrastructure/scheduler/$f")" "$f"
done

# ── 2. BL-9：phase_lifecycle.README.md §3 验收映射表 3/5 条测试不存在
ls -1 eng/tests/runtime/
for p in eng/tests/runtime/test_global_state_spy.py \
         eng/tests/runtime/test_phase3_external_fixture_independent.py \
         eng/tests/runtime/test_no_cross_phase_graph.py; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"
done

# ── 3. BL-9：三个上游约束文档不存在
for p in ACSD_ENGINEERING_CONSTRAINTS.md 03_TARGET_PRODUCT_AND_ARCHITECTURE.md \
         tasks/03_RUNTIME_DATA_IO_TASKS.md; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"
done

# ── 4. §3.9 的「机器读通过」不覆盖 §3 验收：只看 registry 视图泄漏
sed -n '224,235p' lib/infrastructure/scheduler/core/phase_lifecycle.py

# ── 5. CE-15：FNV offset basis 掉了末位 7（全仓 10 处一致 ⇒ 统一约定）
python3 -c "print(14695981039346656037//10 == 1469598103934665603)"   # True
grep -rn "1469598103934665603" lib | grep -v Binary   # 10 处，含本片 4 处

# ── 6. CE-16：206.265 的「精确恒等式」是假的（差 0.94 ppm）
python3 -c "e=(180*3600)/__import__('math').pi*1e-3; print(e, 206.265-e, (206.265/e-1)*1e6)"
grep -n "IPV_ARCSEC_PER_UM_PER_MM" lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp  # → 第 58 行
grep -n "18\.5" docs/science/DATA_SEMANTICS.md                                          # → 1051 行，存在

# ── 7. CE-17：tail[96] 是静默截断不是溢出；阈值 ≥100 GB
python3 -c "b=96;print('digits avail =', b-(len('DATA sha256=')+64+len(' bytes=')+1+1))"

# ── 8. BL-3 / BL-4：consume() 的 3 个失败返回点
sed -n '191,206p' lib/infrastructure/scheduler/src/block_frame.cpp
sed -n '82,90p'   lib/infrastructure/scheduler/src/checkpoint.cpp     # replay 旁路
sed -n '44,47p'   lib/infrastructure/scheduler/src/checkpoint.cpp     # 它旁路的守卫
grep -n "frame.consume" lib/infrastructure/scheduler/src/block_flow.cpp \
                         lib/infrastructure/scheduler/src/normalize_workflow.cpp

# ── 9. BL-5：create() 无 else（244-249），而 4 条失败路径在 block_frame.cpp
sed -n '244,249p' lib/infrastructure/scheduler/src/normalize_workflow.cpp
sed -n '150,155p' lib/infrastructure/scheduler/src/block_frame.cpp

# ── 10. BL-6 / CE-4：budget.py 恒真门（max(0.0,·) 恒 ≥0）
sed -n '425,435p' lib/infrastructure/scheduler/budget.py
sed -n '561,566p' lib/infrastructure/scheduler/budget.py

# ── 11. BL-7：PressurePolicy 零阈值 + 无兜底
sed -n '106,115p' lib/include/acsd/core/memory_pressure.h
grep -n "high_percent\|evict_after_samples\|max<std::uint32_t>(1" \
     lib/infrastructure/scheduler/src/memory_pressure.cpp
grep -n "static_assert" eng/packaging/config/runtime_resources_generated.h.in  # 压力 7 键无守卫

# ── 12. BL-8：台账落盘失败被丢弃且缓冲不清
sed -n '160,179p' lib/infrastructure/scheduler/src/memory_pressure.cpp
sed -n '210,215p' lib/infrastructure/scheduler/src/memory_pressure.cpp

# ── 13. BL-10：注释与代码相反（不驻留 vs push_back）
sed -n '176,178p' lib/infrastructure/scheduler/src/mosaic_window.cpp
sed -n '203,215p' lib/infrastructure/scheduler/src/mosaic_window.cpp

# ── 14. CE-7：want=0 ⇒ 授予全部可用核
sed -n '151,162p' lib/infrastructure/scheduler/budget.py
```

---

## 9. 结论

`INF-scheduler-003` **判定：阻断**。

- 11/13 份由我亲自逐行读完（3384/4026 = **84.1%**）；剩余 642 行（`normalize_workflow.cpp` 1–239/345–503、`mosaic_window.cpp` 1–169/290–369）**未由我亲读**，已由子代理逐行覆盖并逐条复核，证据强度已逐条标注。
- **阻断 11 条**，其中 3 条最重：生产哈希路径的**有符号溢出 UB**（`canonical_hash.cpp:310,313,315`）、两条 **fail-open**（`checkpoint.cpp:82-88`；`block_flow.cpp:213-214` + `normalize_workflow.cpp:293`）、**恒真门且标签说反**（`budget.py:565`）。
- **构造反例 24 条，19 条推翻既有结论**；**6 条推翻我自己的初始假设**（`tail[96]` 非溢出而是截断、`whole*100` 回绕物理不可达、`b->raw()` 空指针不成立、`memory_budget` 溢出安全属实、UTF-8 字节序 = 码点序、`g_current_governor` 非违规单例），**1 条更正子代理的算术误判**（CE-20 的 `_leased` 未越界）。
- **发现 3 类新问题**（不在既有审稿记录、不在给定检查项清单内）：
  1. **`PressurePolicy` 零阈值默认构造即活锁** —— 需同时读头文件默认值、`.h.in` 的 `static_assert` 分布、`max(1,x)` 钳位、以及「`pressure_policy_from_config()` 全仓仅一个调用点」四处才拼得出；
  2. **验收映射表 3/5 条指向不存在的测试**（`phase_lifecycle.README.md:52-54`），且 §5 的 `PHASE_REGISTRY_ISOLATION PASS` 不覆盖该表任一项 —— 正是负责人裁定要抓的「用检查通过冒充证据」形态；
  3. **`budget.py` 的 `ProcessBudgetRegistry` 是 C++ 真本的「无锁 + 吞错 + 已漂移」第二份实现**（BL-11），且唯一验它的 selftest **在「缺锁」这一唯一真实缺陷所在的维度上判别力为零** —— 又一个教科书式自洽式断言。
- 本片**质量分层明显**：`memory_pressure.cpp` / `memory_budget.cpp` 是全片最佳实现（显式自陈 fail-open、「没丢也落盘」、真溢出安全式），`pipeline.cpp` 严谨度次之；阻断项集中，与全片性劣化无关。
- **未发现「私建线程池」**（0 处，3 个代理独立 grep 复核同侧），**未发现「退役对象仍有活调用者」**（反而发现两处反向：`choose_victim` 生产零调用者、`ProcessBudgetRegistry` Python 副本生产零调用者）。
- ⚠️ **上游缺口须登记**：发现恒真门的那两个检查器（`check_taut_null_guard.py` / `check_test_discriminative.py`）已随 G08-01（`e5f589a6`）删除且未重建，只剩 `.pyc`。本片 BL-6 这类恒真门**短期内无任何机器兜底**，只能靠对抗性审核 —— 与负责人裁决方向一致，但应进 `UNRESOLVED_REGISTER`。