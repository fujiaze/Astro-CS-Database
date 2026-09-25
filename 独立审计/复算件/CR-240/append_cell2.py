import io, sys
sys.stdout.reconfigure(encoding='utf-8')

p = r"独立审计/证据/通读-CR-240.md"

body = r"""
---

# 格 2 · `module_adapters.cpp:301-600`

## §2.5 数值处置表（格 2，14 项）

| 位点(文件:行) | 符号或键 | 现行值 | 它是什么（一句话用途） | 处置 | 依据锚 | 待确认时给保守方向与影响范围 | 置信 |
|---|---|---|---|---|---|---|---|
| :369 | `if (workers > 0)` | 0 | 只有正数才写 OpenMP ICV 的门槛 | 结构性不适用 | 非负性判据，不承载科学语义 | — | CONFIRMED |
| :376,:384 | `prev_ > 0` / `prev_ = -1` | 0 / -1 | "未捕获到旧 ICV" 的哨兵值 | 结构性不适用 | 哨兵，非科学量 | — | CONFIRMED |
| :394 | `v[0] == '1'`（ASTROCS_LEASE_TRACE） | '1' | 观测开关只认字面 "1" | 待确认 | 条文面零提及（检索式：`git grep -n "ASTROCS_LEASE_TRACE" -- docs ASTROCS_DESIGN.md` → 零命中；仅本文件 :388-389 自述）。写 "true"/"2" 即为关 | 保守方向＝视为关（现状）；影响范围＝仅租约可观测性，不影响科学值 | CONFIRMED |
| :418 | `%.6f`（nodetrace 时长） | 1e-6 | 执行窗口打印精度 | 结构性不适用 | 打印精度，不参与判据 | — | CONFIRMED |
| :436 | `default: "UNKNOWN(...)"` | — | 未映射状态码的兜底文案 | 结构性不适用 | 文案兜底；`ACS_ERR_STATE`/`ACS_ERR_SELFTEST` 走此支（`common_abi_v1.h:57-65`） | — | CONFIRMED |
| :467 | `kP1D2R` | 0.01745329251994329577 | deg→rad | 公式导出（单位换算定义常数） | π/180；`docs/algorithms/PLATESOLVE.md:341` 点名该参考解。复算：与 `math.pi/180` 相对差 0.0 | — | CONFIRMED |
| :468 | `kP1R2D` | 57.29577951308232087680 | rad→deg | 公式导出（单位换算定义常数） | 180/π；复算相对差 0.0，且 `kP1D2R*kP1R2D==1.0` 精确成立 | — | CONFIRMED |
| :470 | `kP1FitsPixelOrigin` | 1.0 | 内部 0-based 下标 → FITS 1-based 的桥接偏移 | 文献值 | FITS WCS Paper I §2.1.1（CRPIX 为 1-based）；本文件 :471-475 已写明"恰好施加一次"的纪律 | 若某调用点漏加＝恒定 1px 系统偏差（按 0.5″/px 即 0.5″）。施加点 :4671 在段外，**须与 CR-240…CR-250 合并定案** | PARTIAL |
| :483-484 | `(cd11*u + cd12*v) * kP1D2R` | — | 中间世界坐标 ξ/η 由 deg 转 rad | 公式导出 | FITS-WCS Paper I §2.2（pixel→intermediate world）＋ Paper II TAN/gnomonic（:464-466 自引）；我已独立复推 r0/e/n 三向量与 r ∝ r0+ξe+ηn：e=(−sinα0, cosα0, 0)、r0×e=(−sinδ0 cosα0, −sinδ0 sinα0, cosδ0) 与 :490-492 逐字一致 ⇒ 代码按式算，合规 | — | CONFIRMED |
| :498 | `norm > 0.0` / `isfinite` | 0 | 归一化前的退化保护 | 结构性不适用 | \|r0+ξe+ηn\|² = 1+ξ²+η² ≥ 1，该分支实际不可达，属防御性 | — | CONFIRMED |
| :503,:520-521 | `pz` 夹到 [-1,1]、`s` 夹到 [0,1] | ±1 / 0,1 | asin 定义域钳位 | 结构性不适用 | 浮值域钳位（防 asin 返回 NaN），不承载科学语义 | — | CONFIRMED |
| :505-507 | `> 180.0 −= 360.0` / `< −180.0 += 360.0` | 180,360 | atan2 结果落在 (−180,180]，此处做环绕归一 | 结构性不适用 | 角度环绕定义常数 | 注意：本参考解返回的 RA 值域是 (−180,180]，与 WcsTan 惯例 [0,360) **不同域**；唯一消费者 `p1_angular_sep_deg` 用 Δra 环绕（:515-516）故无碍，但若后续段有"直接比较 RA"的用法即错。**须与 CR-240…CR-250 合并定案**（消费者在 :4425/:4671 一带） | PARTIAL |
| :515-516 | `3.14159265358979323846`（π 字面量 ×2） | π | Δra 环绕用的半周 | 公式导出 | 复算与 `math.pi` 相对差 0.0；但同一函数已由 `kP1D2R` 可导出 π（1/kP1R2D），此处第三次复写常数值 ⇒ 属"同一量多字面"而非错值 | 保守方向＝改用具名常量；影响范围＝仅本函数 | CONFIRMED |
| :537 | `uint32_t workers_ = 2;` | 2 | 无调度上下文时喂给 host budget 的缺省 worker 数 | 待确认 | 见 CR-240-05。宪章 §10.4/§10.5（本文件 :352-353、:363 自引"线程数仍唯一来自 host budget，无任何硬编码"）——该自述与本行字面 2 直接冲突 | 保守方向＝改 1（宁串行不越权）；影响范围＝`validate_config`/`inspect` 两条路径（本批查明二者在生产面无调用者，见 CR-240-04 附注） | CONFIRMED |
| :578 | `p.work_units = 1;` | 1 | 会话节点自报工作量 | 待确认 | 见 CR-240-04 | 保守方向＝报真实值或删字段；影响范围＝P2/P3 会话节点的计划面 | CONFIRMED |
| :599 | `lease.acquired() ? lease.size() : 1u` | 1 | 拿不到租约时按单 worker 执行 | 结构性不适用 | 降级方向＝偏保守（不超卖、只变慢），非放行 | — | CONFIRMED |

## findings（格 2）

### CR-240-04 · 会话节点 `plan()` 写下的三项登记全仓零读者，调度器真正读的三项一个都不声明
- 轴：D（主轴）/ C
- 位点：`:576-581`
  `ModulePlan p; p.node_id = node_id; p.work_units = 1; p.parallel_axes = {"tile"}; p.cpu_heavy = desc_.execution_class == "cpu_heavy";`
- 上位依据：`docs/contracts/RT-001.md:28`「`ModulePlan`：node_id、work_units、estimated_memory_bytes、parallel_axes、kernel_ids、cpu_heavy」，同文件 `:4-5`「冻结点：public headers 的接口签名与语义」；`lib/include/astrocs/core/module.h:44-46`「plan() 不得修改输入；只输出工作量、内存、I/O、并行轴与 kernel 请求」＋`:55`「模块必须按**真实**并行度声明（禁硬编码核数）」。设计层零提及（检索式：`git grep -n "并行轴\|work_units\|ModulePlan" -- ASTROCS_DESIGN.md docs/design ENGINEERING_SPEC.md` → 零命中）。
- 现状：唯一的 `ModulePlan` 消费者是 `lib/infrastructure/scheduler/src/runtime.cpp:185-196`，只读三项：`pl.value().min_workers`、`pl.value().max_workers`、`pl.value().estimated_memory_bytes`（`module_reported_memory = ...` :195）。`SessionModule::plan()` 这三项一个都不填（`min/max` 落 `module.h:57-58` 的 `1/0` 缺省、`estimated_memory_bytes` 落 `0`），填的恰是无人读的 `work_units/parallel_axes/cpu_heavy`。
- 差在哪：合同点名的"工作量与并行轴登记"在生产中枢是**只写不读**的面。`git grep -n "parallel_axes"` 全仓命中＝本文件 8 处赋值（:579、:13101-13119、:13341、:15255）＋`module.h:51` 声明＋`RT-001.md:28` 一行＋`eng/tools/quality/check_pipeline_graph.py:192,198` 夹具字符串，**无任何读取表达式**；`cpu_heavy` 同理（4 处赋值、零读取）；`ModulePlan::work_units` 亦零读取（`lib/algorithms/*/src/module_entry.cpp` 里的 `work_units` 是 DLL C ABI 自己的 JSON 键，与 `ModulePlan` 无连接）。
- 后果：①"各节点声明真实并行轴（tile/pixel/source/sub-block…）"这一登记面不构成任何调度事实，合同与代码说的不是同一套数；②真正活的两个旋钮（min/max workers、模块自报峰值内存）在会话节点上恒为缺省，于是 runtime.cpp:214-222 自己声明的 §8.3「模块声明」输入面对 P2/P3 会话节点永久缺位，只剩通用估算器兜底；③`module.h:55` 的"必须按真实并行度声明"对本文件 4 个适配器实现全部不成立。
- 定级：S2 — 触发路径逐跳可写（`lib/infrastructure/cli/commands.cpp:1721` normalize → `runtime.cpp:185-196` → `module_adapters.cpp:576-581`），但不改产品字节，错的是"登记面即事实面"这一对外主张。
- 怎么算修好：二选一并与合同同批改：**(A)** 会话节点如实声明 `min_workers/max_workers/estimated_memory_bytes`（并让调度器真正读用被保留的三项，或删字段）；**(B)** 若 `work_units/parallel_axes/cpu_heavy` 定位为诊断输出，则经 `inspect()/last_manifest()` 落进产品 manifest，并把 `RT-001.md:28` 改注为"诊断字段，不参与派发"。禁止只改一半：`check_pipeline_graph.py:192,198` 夹具与 `eng/contracts/config/module_lifecycle_contract.schema.json` 里出现 `parallel_axes`/`work_units` 字面量，删字段须同批改这两处，否则机器门判红。
- 置信：CONFIRMED（读取面以全仓 `git grep` 双向核过：先找赋值、再找读取表达式）

### CR-240-05 · 无调度上下文路径把 host budget 注成硬编码 `2`，与本文件自称的"线程数唯一来自 host budget、无任何硬编码"直接冲突
- 轴：D（主轴）/ C
- 位点：`:537` `uint32_t workers_ = 2;`；消费点 `:553-554`（`validate_config` 内 `if (!hs.init(workers_))`）、`:657-658`（`inspect()` 内同一句）；注入实现 `:333-335` `astrocs_host_state_set_budget_v1(state, workers, workers, &host);`
- 上位依据：宪章 §10.4/§10.5——本文件 :363 原文"线程数仍唯一来自 host budget（宪章 §10.4），无任何硬编码"、:352-353 引「违反宪章 §10.5『任何连续 10 s 低于 60% 或只有一个活跃计算线程均失败』」；`lib/include/astrocs/core/module.h:55`「禁硬编码核数」；`AGENTS.md §6` 硬禁令「不硬编码线程/ISA/block：由 benchmark 生成的 profile 决定」；`RT-001.md` §2.4「`ThreadBudget::acquire`：原子预留，RAII 归还，**全局不超卖**」。
- 现状：`HostSession::init()` 把同一个 `workers` 同时写进 `budget.available_cpus` 与 `budget.max_workers`，既不取 benchmark profile、也不经 `ThreadLease` 授权。`execute()` 走的是另一条正确路径（:596-601：`ctx.acquire_lease(host_workers)` → `cap` → `hs.init(cap)`；且 `context.cpp:226-238` 的 `acquire_lease` 会把请求收缩到 `dispatch_budget_hint()` 公平份额），而 `validate_config()/inspect()` 直接吃 :537 的字面 2。
- 差在哪：同一个 host 预算有两个来源——租约授权（execute）与字面常数（validate/inspect），后者绕开 `common_abi_v1.h:100-107` 的 `acquire/release` 记账语义。注释 :330-332 自辩"仅影响 host budget 上限，不影响门禁语义"——上限本身就是"不超卖"的护栏，这句话把护栏说成了无关项。**方向：放行侧**（无授权即取得 2 个 worker 的名义额度），不是保守侧。
- 后果：会话内核任何按 `host->budget.max_workers` 决定并行度的实现，在 validate/inspect 路径上会开出未记账的 2 worker（`IModule::inspect` 在 `module.h:86-87` 标注"线程安全：可并发"）⇒ 进程实际线程数可超唯一预算 ⇒ 宪章 §10.5 的 worker/利用率观测归因失真，"worker 数＝授权数"这一对外主张不再成立。
- 定级：S2（缺"会话内核确按 max_workers 起线程"的下游一跳取证；且本批查明这两个方法在生产面无调用者 ⇒ 若合并段后确认始终不被调用，本条降为"死面上的错误常数＋自述注释与代码相反"）
- 怎么算修好：①删 :537 的字面 2，validate/inspect 显式按"纯读、无并行"注 1（保守侧）；②若保留参数，须由调用方（Runtime）从 profile/budget 传入，并同批订正 :330-332 与 :363 两句自述；③把该缺省值登记进配置合同面，避免"兜底值无处可查"。
- 置信：PARTIAL（未逐行读 `lib/phase1_session/p1_session.cpp` 内 `ac_set_num_threads` 的真实消费点；:334-335 注入面与 :596-601 对照面已核）

### CR-240-06 · `run` 失败丢弃会话 `last_error`、`inspect` 失败返回码被静默忽略，两条错误信道可同时为空
- 轴：R（主轴）/ D
- 位点：`:639-650`
  `st = fn_run(h, cfg);` … `{ acs_span_u8 man{}; if (fn_inspect(h, &man) == ACS_OK && man.data) { manifest_ = ...; } }` … `if (st != ACS_OK) { fn_destroy(h); return to_result(st, "session run"); }`
  对照同一函数内 validate 分支 `:632-638`：`std::string why = fn_last_error ? fn_last_error(h) : ""; if (why.empty()) why = "session validate: " + status_str(st);`
- 上位依据：本文件 :284 自述「session C++ 辅助（last_error 保留错误细节；RT-008 CLI 合同需要）」；:640-641 自述「失败时 manifest 含 error_kind 供 CLI 按 04 合同映射退出码」；`docs/contracts/PUBLIC_API.md:721-723` 逐条登记会话五函数的拒绝语义；`ASTROCS_DESIGN.md §7.2` 退出码映射（本文件 :87 即引"磁盘满失败瞬间分类 (§10 + §7.2 exit 10)"）。
- 现状：validate 失败**保留**细节（:634-635），run 失败只回 `to_result(st, "session run")`＝`"session run: INTERNAL"`（:440-454 的 `to_result` 只把 `acs_status` 映成域＋一个词，`Error` 结构里没有会话细节字段）；:644 的 `fn_inspect` 返回码只在成功分支被用，失败既无 `else`、不记 log、不置标志。
- 差在哪：run 失败时 CLI 的退出码映射唯一细节来源是 manifest，而 manifest 的唯一写点被一个被丢弃的返回码守着——inspect 一旦失败（会话内部错误常同时使 inspect 失败），两条细节信道同时为空 ⇒ 归因信息在最后一跳被丢掉。判据方向：红/红，不变；变的是**分类方向**（具名 DATA/IO 退成笼统 INTERNAL）。
- 后果：`normalize/mosaic/export` 任一会话节点在 run 阶段失败时，CLI 退出码与 `run_manifest` 的错误分类失真（§7.2 映射合同不再成立）；排障面无法区分"配置类失败"与"内部崩溃"。另外 `:683-688` `last_manifest()` 在 manifest 为空时报 `": no manifest captured (execute not run)"`——execute 明明跑过，文案把"inspect 失败"误称"未执行"，会把排障带向错误结论。
- 定级：S3（不改产品字节；影响限于退出码与诊断归因）
- 怎么算修好：①`:650` 与 `:622` 改成与 validate 分支同构（先 `fn_last_error(h)` 再 `fn_destroy(h)`，空则回落 `status_str`）；②`:644` 给 inspect 失败补 `ctx.log(LogLevel::WARN, ...)` 并把原因并进 Error；③`:685-686` 文案区分"未执行"与"执行了但无 manifest"；④同批给 CLI 退出码映射补一条"run 失败 + inspect 失败 ⇒ 仍具名域"的正/负例，否则改文案会先把钉文案的门判红。
- 置信：CONFIRMED（分支原文与 `to_result` 定义逐字核过；未实机跑 CLI 验证最终打印）

## 本格"核过但不报"的项与检索式

- `:292-317` `shared_work_executor` 的 weak/strong 注册表：`it->first.expired()` 回收与"同一预算源→同一池"的 `entry.first.lock() == budget` 比较已核（`shared_ptr` 比较按对象）；`:314` `created.ok()==false → return nullptr` 是"分不到池⇒串行"的**保守**方向，不构成放行。
- `:364-386` `ScopedOmpWorkerInjection`：`#ifdef _OPENMP` 两侧对称，异常路径由析构覆盖；它保存 `omp_get_max_threads()` 而非 nthreads-var 原值（OpenMP 无该 getter），属已知近似且方向为"不高于原值"，不报。
- `:407-421` `p10_monotonic_s`/`P10NodeTraceGuard`：纯观测（`steady_clock`），不改调度/科学；`BEGIN` 打印点在段外。
- `:423-454` `status_str`/`to_result`：11 个 `ACS_ERR_*` 全覆盖（对照 `common_abi_v1.h:53-66`），非 OK 一律 fail，无"错误当成功"分支；细节丢失单记 CR-240-06。
- `:456-475`＋`:477-523` 独立前向 TAN 参考解的**数学**我已独立复推并与 :490-492/:494-496 逐式一致 ⇒ 不报"参考解不独立/与 WcsTan 同源"；旧恒真门 `sky2pix(pix2sky())` 已被它替换（`docs/algorithms/PLATESOLVE.md:341` 点名本函数），新门门限与施加点在 :4425/:4671 一带（段外）⇒ 进合并待办清单。
- `:562-566` `cfg.head.struct_size = sizeof(cfg)` 与 `common_abi_v1.h:22-24`「struct_size = sizeof(具体结构)」＋`ACS_SPAN_U8` 宏同义（`sizeof(acs_span_u8)==sizeof(cfg)`）⇒ 一致，不报。
- `:626` `cfg.count = static_cast<uint64_t>(config_.size())`：`config_` 由 `plan()` :575 写入，Runtime 在 execute 前必然先 `plan()`（`runtime.cpp:257`）⇒ 不存在"空 config 进会话"的窗口，不报。
- `:632-638` 的 validate 失败分支本身（细节保留＋`fn_destroy` 后返回）写法正确，仅被用作 :650 的对照。
"""

with io.open(p, "a", encoding="utf-8") as f:
    f.write(body)
s = io.open(p, encoding="utf-8").read()
s = s.replace("<!-- PROGRESS: 1/5 -->", "<!-- PROGRESS: 2/5 -->")
io.open(p, "w", encoding="utf-8").write(s)
print("cell2 appended, progress -> 2/5")
