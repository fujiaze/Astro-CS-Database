# 审稿-P1-INF-cli-001（G08-05 对抗审稿 第 1 遍）

- 片号：`INF-cli-001`（层 `lib/infrastructure/cli`）
- 仓库：`/workspace/Astro CS Database`
- 实际 HEAD：`1fa477a7`（「移除实验域运行结果归档，报告迁入单元文档目录」）——⚠️ 与派单书所写 `850a9ede` **不符**，本报告全部结论基于 `1fa477a7` 实读。
- 纪律：零 git 写；未编译、未跑 ctest/pytest/构建/任何二进制；未读 `/tmp/acsd_g08/`；未改任何仓内文件（本文是唯一新增文件）。

---

## 1. 读完了吗

**口径**：成员份数 = `片清单-权威版.yaml` 中该片 `成员文件` 条目数；「读完」= 用 read 工具自首行读至末行（非 grep 抽样、非摘要）。

| 口径 | 数值 |
|---|---|
| 成员份数（权威清单） | **33** |
| 实际读完份数 | **33** |
| 成员总行数（权威清单 `实际行数`） | **9588** |
| `wc -l` 实测总行数 | **9588**（与清单逐份一致，33/33 无偏差） |
| 实际逐行读完行数 | **9588** |
| **覆盖率** | **33/33 份 = 100%；9588/9588 行 = 100%** |
| 未读完的 | **无**（0 份、0 行） |

口径说明：清单 `实际行数: 9588` 与我逐份 `wc -l` 之和完全相等，故「读了多少行」无口径分歧；本片不含二进制、不含 `.pyc`、不含生成产物（`.h` 生成物不在清单内，其模板 `.h.in` 在清单内且已读）。

---

## 2. 本片判定

### **需修**（BLOCKER 存在但均可定位到单点、无需重构；且不含数据损坏类不可逆问题）

> 口径说明：按「是否需要跨模块重构 / 是否已产出错误科学数据」分级。本片 6 条阻断项全部是**判别力失效**与**fail-open**，不涉及科学数值错误或产物损坏，故判「需修」而非「阻断」。若负责人认为「唯一 in-process 硬门 fail-open」（F-B3）已达阻断级，可上调。

### 最重 3 条

1. **F-B1｜恒真产物（自洽式断言）：`worker_balance.csv` 的 `utilization_pct` 列恒等于 50.00**
   `resource_recorder.h:368-369` 用 `active_workers / (active_workers + runnable_workers)` 计算利用率，但**唯一生产写点两处都把同一个值传两次**：`commands.cpp:941` `set_workers(planned_start, planned_start)`、`commands.cpp:960` `set_workers(eff, eff)`。⇒ 被测量与期望量是同一个变量，`util = eff*100/(2*eff) = 50.00`，对每行、每次 run 恒成立。产物头注释（`resource_recorder.h:362`）称该列用途是「供不平衡分类」——该列**没有第二个取值**。同族的 `resource_timeseries.csv:runnable_workers` 列同样恒等于 `active_workers`。**这是本片最干净、且经我与两个子代理独立复现的同义反复样本。**

2. **F-B2｜唯一 fail-closed 硬门 fail-open：`probe_writable` 丢弃 `fclose` 返回值**
   `disk_gate.h:231-239`：4096 字节 `fwrite` 进 glibc stdio 全缓冲后**返回成功**，真正的 `write(2)` 只在 `fclose` 刷出时才发生，而 `std::fclose(f);`（`:236`）返回值被丢弃 ⇒ `pr.err` 保持陈旧值（`fwrite` 前未清零，通常为 0）⇒ `classify_write_failure(0)` 走 `case 0: return WriteFailureKind::None;`（`:199`）⇒ `write_failure_is_resource_exit(None)` = **false** ⇒ 磁盘满被判成 exit 7 而非契约冻结的 exit 10。这是契约 `resource_gate_v1.json:79-83` 声明的**唯一 in-process 硬门**。同仓 `resource_recorder.h:307-310` 与 `memory_report.h:281-282` 都写了「旧实现忽略返回值 = fail-open」并**已修**，`disk_gate.h` 是这一轮唯一漏改的。

3. **F-B3｜私建线程池 + 其治理机制在生产中零调用者**
   `commands.cpp:1506-1519` 在会话层构造 `std::vector<std::thread> hash_pool`，N 个线程用 `next_hash.fetch_add(1)` 动态领取 artifact 哈希任务 —— 与仓内**持 Runtime 租约**的池（`lib/algorithms/sampling/module.yaml:31`、`lib/algorithms/upm/module.yaml:42`）形态逐字相同，但**不取任何租约**。而本该拦截它的 `runtime_contract.h:215/246` `register_source` / `request_lease` 在 `lib/` 下**零生产调用者**（子代理 grep 证据），生产侧唯一引用是只读的 `mode_gate.h:56-57`。⇒ §10.4「不私建线程池」在本片是**装饰性的**：每个 `v6_mode_route` 事件恒发 `budget_source_owner:""` / `budget_allocated_cores:0`，同时在同一事件里断言 `one_budget_source_rule`。另注：该注释（`commands.cpp:1483-1485`）称「N = Runtime 预算核数 budget」，而 `budget` 实绑于 `cli_affinity_cpu_count()`（`:1466`），且工作是整文件哈希（I/O 密集），用核数做单位本身即错。

---

## 3. 逐文件清单

「读了什么」→「看到什么」→ 判定。全部 33 份。

| # | 文件 | 行 | 读了什么 | 看到什么（关键 `文件:行`） | 判定 |
|---|---|---|---|---|---|
| 1 | `commands.cpp` | 2734 | 三命令会话层、run_manifest、运行图、资源门调用点、dispatch 全链 | F-B1/F-B3 源点 `:941/:960/:1506-1519`；私建池；`queue_depth_observed` 恒真 `:1315`；契约要求但 C++ 从未产生的 `allocated_capacity_undeclared` 无任何登记；恢复哈希扫描被**任一**畸形历史 manifest 永久毒化 `:1718`；`--phases` 死码 300 行 `:1960-2191`；6 处 `catch(...){return ".";}` 把产物落 CWD；崩溃路径 `main.cpp:72` 二次 `make_run_id()` | **FAIL** |
| 2 | `parser.cpp` | 705 | 命令解析、config 白名单/形态校验、`file_sha256` | `file_sha256:216-227` **无 `f.bad()` 检查**，I/O 中途失败时对**前缀**算哈希并置 `ok=true`；`local_cpu_signature:254` 零调用者且硬编码 `"amd64"`；`git_head_sha:231` 相对 CWD 且不认 `packed-refs`；`session_keys()` 含 7 处「生产零读取、死键台账已登记」的透传键 | **FAIL** |
| 3 | `memory_report.h` | 624 | RSS/allocation 报告、Theil-Sen、reclaim 裁决、自校验 | `decide_allocation_reclaim:185-186` **peak ≤ 32 MiB 时回落门恒真**；`validate_alloc_report:611-617` **用同一 `theil_sen_slope_xy` 复算同一 JSON 的同一斜率**（自洽式断言）；`:482-487` NaN 斜率落 else → 判 **Stable**；`reclaim_frac` 名实相反（实为 `(peak-last)/peak` 留存率）；JSON 键 `*_mb_per_s` 载 MiB 值 | **FAIL** |
| 4 | `runtime_client.cpp` | 583 | IR 构建、`run_pipeline`、错误域→码 | 头部自称「退出码唯一源」却硬编码 7 个字面量 `:406/:442/:456/:476/:544/:545/:558`；`:394-396` 取消只 `break` 不返回 ⇒ **管线照跑到底**；`:541` 空 catch 使 `disk_full` 漏扫 → 7 而非 10；`:574` `artifacts` 无 `is_array()` 守卫；`:148` 引 `eng/ci/…`（目录不存在）；`last_pipeline_ir_json:375` 返引用**不持锁** | **FAIL** |
| 5 | `resource_gate.h` | 561 | 全部门禁判据 | `gate_enforcement:268` **恒返回 RecordOnly、两个形参被注释掉**；`:15-16` 与 `:53` 仍宣称「门禁失败 → exit 10」「→ 协作取消」= **过期注释**；`:105` 自称「本文件不再出现字面量阈值」却有 `:207/:336/:363/:368-370` 七处；`:233` `>=` 与契约「**严格大于** 10 s」及常量名 `…Exclusive` 矛盾；`:343-347` 单线程判据用 `selected OR max_active`（契约规定回退统计量=peak）；`:519` 诊断写 `>` 判据用 `>=` | **FAIL** |
| 6 | `subcommand.h` | 518 | 预检页、确认、`print_template`、`dispatch` | `print_template:482-484` `f<<text; if(!f.good()) return IO;` —— **无 flush、无落盘字节核对**，析构 flush 失败却已返回 0（`commands.cpp:2071` 同型）；全部 `is_cancelled()` 只在 `ACSD_TEST_*` 钩子里；`confirm_run` EOF → `ARGS(2)` 而非 `CANCELLED(9)` | **FAIL** |
| 7 | `monitor.h` | 476 | /proc 采样、`ProcessMonitor` 摘要 | `pss_bytes:72` **无任何生产者**（`read_proc_self` 只开 status/io/stat + sysinfo，从不读 smaps_rollup）⇒ 恒 0，而 `runtime_contract.h:309/329` 把它列为契约必含；`:108` 文档「返回 false 仅当无法打开关键文件」vs `:222-223` `(void)ok; return true;` **恒真**且唯一调用点丢弃返回值；`:426` `size()>=2` 门把 totals 归零（而 `:416-420` 注释显示作者**已在姊妹分支修过同一 bug 类**）；`:441` RSS 斜率分母用 `N*interval` 而真跨度是 `(N-1)*interval`（N=2 时恰差 2×）；`:344-346` 三个 `uint64` 差值无钳位（`:343` 的 rss 却钳了）；`:257` 硬编码 `/100.0` 与 `:277` `sysconf(_SC_CLK_TCK)` 同文件两种口径 | **FAIL** |
| 8 | `session_commands.h` | 429 | 会话路由、`input_contract`、模板生成 | `session_of:43` 未知名返回 `SessionId(0)`，`input_contract:96` `default:` → **静默套用 EXPORT 合同**（fail-open）；`render_checks:420-423` 未知 level → 渲染成绿色 `[correct]`；`has_error:411` 精确匹配 `"error"`；模板自相矛盾：`config_fields:224` 发 `"hips_paths": []`、`:267` 发 `{"hips_dir": ""}`，而**本文件自己的** `subcommand.h:116/:109` 拒绝空数组与空串 ⇒ **`--template` 产物被自身预检拒绝**；`:173/:233/:245/:284` 引 `eng/ci/ledgers/dead_config_keys.json`（目录不存在） | **FAIL** |
| 9 | `runtime_contract.h` | 406 | §10.4 预算登记、§10.5 字段面、SO-05 | `required_metric_keys():326` 列 14 名 vs `heavy_metrics_complete():336` 查 9 名，**仅 6 名重合**；`:347` `active_window_seconds >= 0.0` 在默认 `0.0` 上**恒真**（空断言）；`:267-270` `release_lease` 过量释放把 `leased_total_` 归 0 = **把全部预算还回去**；`reset_for_test()` public；`:26` 称 `--strict-resource-gate` 供旧测试断言 rc=10，与 `commands.cpp:1334`「恒 false」直接矛盾；`:174`/`:185` 两处 `eng/ci/…`、`runtime_mutation_driver.py` **悬空**（后者是自称「注入锚点」的引擎，`.py` 已删只剩 `.pyc`） | **FAIL** |
| 10 | `resource_recorder.h` | 378 | 记录器、三产物落盘 | `write_all:368-369` F-B1；`:128/:154-159` 用**名义** `interval_` 归一 CPU 而非真实墙钟（方向单一：只让利用率虚高）；`:172` `n_samples()` 无锁读 `n_`（`:161` 加锁写）；`:319` `run_id` 未转义插 JSON；`:357-359` cfitsio 探针缺失时 JSON 段整块省略且**无「未采样」标记**（与 `memory_report.h:20-21` 自订纪律矛盾）；`:284` 引 `eng/ci/root_manifest.json`（不存在） | **FAIL** |
| 11 | `jsonl.h` | 330 | 事件发射器、脱敏、4096 行上限 | `fit_event_line:124` 无自由文本可裁时 `break` 并**照样发超长行**；`:117` 8 次迭代后静默放弃；`human_summary_of:146` `clip_utf8(s,4095)+"\n"`，而 `clip_utf8:105` **在填满预算后再追加 3 字节 `…`** ⇒ 最长 **4099 字节**，违反自称的 4096 上限（我另证 `kEventLineMaxBytes`/`clip_utf8`/`fit_event_line` **全仓零测试覆盖**）；`:189-194` 协议拒发**不影响退出码** ⇒ 可 rc=0 而 stdout 无 `final`；`exit_code_for_write_errno:305` **只映射 ENOSPC**，漏契约点名的 EDQUOT/EFBIG | **FAIL** |
| 12 | `disk_gate.h` | 274 | 磁盘余量探测、写失败归类、预检文案 | F-B2 `:236`；`:6` **伪引**「ACSD_DESIGN §6.3 退出码表」——实测 `:383 ### 6.3 投影算法` 讲 TAN/SIN/CAR 等八种投影，零字涉退出码；`:7-8` 有一段无出处引语（`docs/` 零命中）；`:254/:266/:269` `/(1024*1024)` 整除截断 ⇒ 「可用 0 MiB < 需求 0 MiB」；`:87/:54` 两个魔法数；`path_bytes_bounded:129` 不存在路径静默 `return 0` | **FAIL** |
| 13 | `process.cpp` | 215 | POSIX fork/exec + Windows CreateProcess | `:166-172`/`:87-92` **devnull 打开失败被静默跳过** ⇒ 子进程继承父 stdout，渲染器噪声污染 JSONL，而 `ok():213` 仍返回 true（fail-open）；`acsd_process.h:29` 承诺「超时杀**进程树**」，实为 `::kill(pid, SIGKILL)` **只杀直接子进程**（无 `setpgid`/`killpg`/Job Object）；`acsd_process.h:19` 引用**不存在的 `status` 枚举**；`WIFSIGNALED` 与兜底三分支的三个 bool **全 false**；Windows `SetEnvironmentVariableA:70` 改父进程环境且**从不还原**；POSIX `fork` 后 `setenv:164` 非 async-signal-safe | **FAIL** |
| 14 | `command_tree.h` | 166 | 唯一命令树表 | `boolean_flags:34` / `value_flags:55` **零消费者**（我 grep 确认只有定义 + 两处注释），真正在用的是 `parser.cpp:38-56` 的**手抄孪生表** ⇒ 本文件 `:3-5` 与 `README.md:37-38` 的「唯一事实源」被证伪；`--config` **不在任何命令白名单**，而 `commands.cpp:1966/2007/2049/2086` 四处读它 ⇒ validate/plan/inspect 三个命令**不可达**；`:44-47` 与 `runtime_contract.h:26` 对 `--strict-resource-gate` 能否产 rc=10 说法相反 | **CONDITIONAL FAIL** |
| 15 | `protocol.h` | 152 | 事件协议 v1 冻结校验 | `:19-20` 自称「字段数经 sizeof 推导, 不写字面量」而 `:22` 就是 `names[10]`；`:117/:143` `get<long long>()/get<int>()` 对超范围无符号值**抛异常**，逃逸到 `main.cpp:68` → INTERNAL(70)；`:95` 未登记 kind 返回「无缺失」= fail-open（当前被 `:127` 兜住，但函数是 public inline）；**不校验任何字段值**（`schema_version:"99"` 可过）；`kEventFieldCountV1` 零消费者 | **CONDITIONAL FAIL** |
| 16 | `cli_common.h` | 151 | 共享声明、benchmark 判据 | `:39` 称 `utf8_from_wide_*` 由「main.cpp wmain 与 **commands.cpp cli_install_dir** 共用」——`cli_install_dir` 用 `GetModuleFileNameA`，**从不调该组函数**（我 grep 确认活调用点只有 `main.cpp:90/94/96`）⇒ 悬空引用；`:74-75` `kBoolFlags`/`kValueFlags` **声明无定义无引用**（改名遗留化石）；`:22-35` `benchmark_profile_verdict` 对非字符串 `correctness_test` 抛 `type_error.302` 未捕获 ⇒ 用户畸形输入得 70 而非 3；`:104-117` 声称 mosaic/export 块面「显式不支持」，实现却已完整接线 | **FAIL** |
| 17 | `memory_growth.h` | 136 | 内存增长/泄漏分析 | **默认配置下 `MemDiag::Leak` 不可达**：`:37` `leak_slope_bytes_per_iter=0.0` ⇒ `:104` 守卫退化为 `slope_abs < 0.0` 恒假、`:116-117` 的 `> 0` 恒假 ⇒ `:124` 把**任意正斜率**（1 B/轮与 1 GiB/轮无异）一律判 `Growing`；`:105` 振幅分类会把**锯齿状真实泄漏**判成 `Oscillating` 并写死 detail「非泄漏」；`:4` 自称「禁止硬编码」却有 4 个魔法数；`:99/:107` `static_cast<int>(0.85*100)`=84、`(0.3*100)`=29，**报值比判定阈值各少 1**；**全仓零生产消费者** | **FAIL** |
| 18 | `main.cpp` | 114 | 入口壳、crash boundary、SIGPIPE | `:71-77` 崩溃路径**再调一次 `make_run_id()`** ⇒ CRASH 行的 run_id 与实际事件流/manifest 的 run_id **不同**，无法关联；`mallopt` 返回值全丢、`M_ARENA_MAX=2` 把全部 worker 线程挤到 2 个 arena（与「worker 数由 lease 决定」相悖）；`:95` `WideCharToMultiByte` 转换返回值丢弃；`kHelp` 在 `:60/:66` 打到 **stderr** 而 `commands.cpp:2520` 打到 **stdout**（stdout 契约是纯 JSON/JSONL） | **CONDITIONAL FAIL** |
| 19 | `mode_gate.h` | 98 | V6 显式模式路由门 | `:36-42` config 读/解析失败被 `if(f)`+`catch(...){}` 吞掉 ⇒ `have_doc=false` ⇒ `:78` 整段判据**不执行**，含 `weight_mode` 的非法 config 通过模式门；`:60-70` 丢弃 `ev.emit`/`fprintf` 返回值；`:56-57` 因预算登记零写者而恒发空 owner / 0 cores | **FAIL** |
| 20 | `resource_events.h` | 88 | stage/resource 事件组装 | `:83-86` `is_unannotated_priority` 的 5.0 s 是**第三个**「未标注」定义（另两处在 `resource_gate.h:336/:397`），且此处按**字符串**分类而门按**布尔** `has_stage_annotation`（后者被 `commands.cpp:1159` 硬编码 true ⇒ 该判据生产永不可达）；`summarize:55-69` 纯字段搬运无独立判据；`:4` 引用已删除的 `build_curve()`/`curve_points` | **CONDITIONAL FAIL** |
| 21 | `runtime_client.h` | 73 | client 声明 | `:18-20` 自认保留 F-EXIT-MAP 偏差（CONFIG/BACKEND→70、RESOURCE→5）——即 `exit_codes.h` 唯一源被本层绕过；`:39` `memory_budget_percent` 默认 0 = 不启用内存回压，注释已自认；`:68` 返引用契约无线程安全说明 | **CONDITIONAL FAIL** |
| 22 | `profile_store_bridge.h` | 48 | profile_store 桥接 | 转发 `save_profile_atomic_v1` / `default_profile_path_v1`，**未复制判据**；被 `commands.cpp:42/202/2664/2680` 真实消费。**本片唯一无实质缺陷的成员** | **PASS** |
| 23 | `README.md` | 45 | 模块 README | `:43` 引 `eng/tools/check_cli_command_layer.py` —— **不存在**（仅 `run/**` 归档树有 12 份拷贝）；它名下的三项保证（命令树完整 / 旧命令 rc=2 / 退出码稳定）因此**无任何机器检查拥有**；`:37-38`「parser.cpp 只从该表取白名单」被 `parser.cpp:38-56` 手抄表证伪；`:39` 「只被 commands.cpp 消费」低报（实为 4 个 TU）；落位表 5 条 / 实有 33 文件 | **FAIL** |
| 24 | `acsd_process.h` | 45 | 子进程合同 | `:29` 「杀进程树」**未实现**；`:19` 引不存在的 `status` 枚举；`:5-6` 「负数错误码」实际只有 `exit_code=-1` 一个值、三种异常共用；`:7` 写 `execvpe`（glibc 无此函数，实为 `execvp`）；`:33` 声称「可并发调用」被 Windows 环境变量改写破坏 | **FAIL** |
| 25 | `resource_gate_thresholds_generated.h.in` | 44 | G-RES-01 阈值生成头模板 | **生成器是活的**（`CMakeLists.txt:149-168`，`string(JSON)` 无 `ERROR_VARIABLE` ⇒ 键缺失即 FATAL，fail-closed 正确）；但 `:10`「每次 configure 重新生成」的承诺**无 `CMAKE_CONFIGURE_DEPENDS` 支撑**——我实测全仓零命中，且 `build/resource_gate_thresholds_generated.h` 与 `build/linux-control/` 两棵树仍是 `namespace astrocs`（模板已改 `acsd`）⇒ **陈旧生成物已实证**。另：契约的语义键（`queued_work_predicate`、`zero_denominator_effect`、`growth_requires_unexplained_reclaim` 等）**无一进入 C++** | **FAIL** |
| 26 | `CMakeLists.txt` | 41 | 模块 target 清单 | `:5-15` 把三步接线写成「待 INT-001 登记」，实际根 `CMakeLists.txt` 早已全部完成（并自称「接线步 1/3/2/3/3/3」）；`:28-41` 声明的 3 个 `acsd_cli_subcommand_*` INTERFACE target **全仓零消费者**，而 `:17` 的理由正是「便于模块化引用与共址测试引用」——理由被 grep 证伪 | **FAIL** |
| 27 | `cancel_token.h` | 41 | 协作取消令牌 | `:32-34`/`:23-27` 信号处理器**只置位不升级** ⇒ `runtime_client.cpp:507` 的无界 `join()` 遇不响应 cancel 的 `run()` 时**Ctrl-C 杀不掉进程**，须外部 SIGKILL；只装 SIGINT/SIGTERM，**无 SIGHUP**；`:15-18` 函数内 static 的首次 `__cxa_guard_acquire` 可能落在信号上下文；`:26` 承诺的「退出码 9」无任何代码强制 | **FAIL** |
| 28 | `module.yaml` | 35 | 模块元数据 | `:13` `entrypoint: acsd::cli::cmd::run` —— **该符号不存在**（全仓唯一命中就是这一行本身）；`:1` 引 `ENGINEERING_SPEC §4` 与 `11_MODULE_SOURCE_TEST_STANDARD.md §4` —— **两份文件都不存在**；`:9/:11` 声明 `abi_version`/`dll_name` 与 `:12 IMPLEMENTED_HEADER_ONLY`（不产出二进制）自相矛盾；`module_id: acsd.infrastructure.cli` 在全仓**只出现 2 行**（本文件自身）⇒ 该模块不在任何登记表/registry 页内，也就逃过一切模块计数 | **FAIL** |
| 29 | `exit_codes.h` | 21 | 11 个退出码唯一源 | `:2` 「本文件是 11 个退出码在仓库内的唯一定义处」**被两处证伪**：`lib/include/acsd/core/contracts.h:42-46` 第二份同字面量枚举；`orchestrator.h:145-157` `AstroCsExitCode` 第三份且**逐值语义不同**（10=CANCELLED vs RESOURCE）。`COMPUTE=6` **全仓零返回点**却在 `protocol.h:34` 与 `docs/detail/infrastructure/19_runtime.md:94` 被登记为既有行为 | **FAIL** |
| 30 | `export/export.h` | 18 | export 薄入口 | 纯 `Subcommand` 描述符，无逻辑。**PASS** | **PASS** |
| 31 | `mosaic/mosaic.h` | 18 | mosaic 薄入口 | 同上。**PASS** | **PASS** |
| 32 | `normalize/normalize.h` | 18 | normalize 薄入口 | 同上。**PASS** | **PASS** |
| 33 | `version_generated.h.in` | 3 | 版本生成头模板 | `#define ACSD_VERSION_STRING "@ACSD_VERSION_STRING@"`；生成器**存在**（根 `CMakeLists.txt:95` + `eng/tools/gen_version.py`）⇒ **未**被删机读删掉。唯一小瑕：acsd target 显式把 `resource_gate_thresholds_generated.h` 登记为构建输入却未登记同类的 `version_generated.h` | **PASS** |

---

## 4. 发现清单

### 4.1 阻断（BLOCKER，6 条）

| ID | 位置 | 问题 |
|---|---|---|
| F-B1 | `resource_recorder.h:368-369` + `commands.cpp:941,960` | **恒真产物**：`utilization_pct` 恒 50.00（自洽式断言，被测量=期望量） |
| F-B2 | `disk_gate.h:231-239`（`:236`） | **唯一 in-process 硬门 fail-open**：`fclose` 返回值丢弃 → 磁盘满判成 exit 7 |
| F-B3 | `commands.cpp:1506-1519` + `runtime_contract.h:215,246` | **私建线程池**，且 §10.4 拦截机制生产零调用者 |
| F-B4 | `memory_report.h:185-186` | **恒真门**：peak RSS ≤ 32 MiB 时回落判据恒为 `Reclaimed`（相对判据 ∨ 绝对容差，绝对项在小进程上吞掉相对项） |
| F-B5 | `commands.cpp:1159,1202` | `has_stage_annotation`/`monitor_present` **硬编码 true** ⇒ `UnannotatedPriority` 生产永不可达；`queue_depth_observed`（`:1315`）恒为 1（`queue_low_run_seconds` 由 `+=0.5`/归零构造，恒 ≥ 0）⇒ 两个自证字段 |
| F-B6 | `eng/tools/quality/__pycache__/` 下 **229 个 `.pyc` 无 `.py`** | 治理事件：被判别力/同义反复类缺陷本应各有一台机器检查（`check_test_discriminative`「绿/红判别性」、`check_taut_null_guard`「同义反复/空守卫」、`runtime_mutation_driver` 变异注入驱动），**这些检查器已不在仓里**，而 `runtime_contract.h:185-186` 仍把 `runtime_mutation_driver.py` 称作「注入锚点，逐字保持」 |

### 4.2 须修（MUST-FIX，19 条）

| ID | 位置 | 问题 |
|---|---|---|
| F-M1 | `disk_gate.h:6`（扩散至 `resource_gate.h:4`、`resource_gate_v1.json:89`、`mon004_enforcement_test.cpp:9-10`） | **伪引**：`ACSD_DESIGN.md §6.3` 实为「投影算法」（`:383`），不含退出码表；真表在 `ERROR_HANDLING_STANDARD.md:98` |
| F-M2 | `parser.cpp:216-227` | `file_sha256` 无 `f.bad()`：中途 I/O 错误返回**前缀哈希**且 `ok=true`；该哈希进 run manifest 与 `verify` |
| F-M3 | `monitor.h:72` | `pss_bytes` **无生产者**恒 0，却是 `runtime_contract.h:329` 契约必含键；单测 `mon002_alloc_test.cpp:34` 用 `s.pss_bytes=rss` 伪造值掩盖 |
| F-M4 | `monitor.h:108` vs `:222-223` | `read_proc_self` 文档承诺的失败信号**恒不发生**（`(void)ok; return true;`），唯一调用点丢弃返回值 |
| F-M5 | `monitor.h:426` vs `:416-420` | `size()>=2` 门使 1 样本 run 的 `total_read/write_bytes`、`avg_equivalent_cores`、`rss_slope` 恒 0；**同一函数的姊妹分支已被作者按同一理由修过** |
| F-M6 | `monitor.h:441` | RSS 斜率分母用 `N*interval_` 而真跨度 `(N-1)*interval_`（N=2 时恰差 2×） |
| F-M7 | `monitor.h:344-346` | `d_read/d_write/d_ctx_switches` 为 `uint64` 减法**无下钳**，单次读失败即回绕至 ~1.8e19（`:343` 的 rss 却钳了） |
| F-M8 | `jsonl.h:146` + `:105` | 自称 4096 字节单行上限，`clip_utf8` 在填满预算后追加 3 字节 `…` ⇒ 可达 **4099**；三函数**零测试覆盖** |
| F-M9 | `jsonl.h:305-307` vs 契约 `resource_gate_v1.json:80` | 写失败→码只映射 `ENOSPC`；契约点名 **ENOSPC/EDQUOT/EFBIG** 三者 ⇒ 同一物理条件在两条路径上分别给 10 与 7 |
| F-M10 | `runtime_client.cpp:394-396` | 取消只在 `ACSD_TEST_PIPELINE_SLEEP_MS` 钩子里被 `break` 检出，**随后完整管线照跑** |
| F-M11 | `runtime_client.cpp:541` | 空 catch 使畸形 manifest 逃过 `error_kind` 扫描 ⇒ `disk_full` 漏判 → 7 而非契约要求的 10 |
| F-M12 | `runtime_client.cpp:406/442/456/476/544/545/558` | 文件头自称退出码唯一源却硬编码 7 个字面量 |
| F-M13 | `subcommand.h:482-484`、`commands.cpp:2071-2072` | `f<<text; if(!f.good()) return IO;` —— 无 flush/无落盘字节核对，析构 flush 失败仍返回 0；`commands.cpp:583-591` 的长注释证明**这一 bug 类已被识别并在 manifest 写入处修过**，这两处漏改 |
| F-M14 | `runtime_contract.h:326-352` | `required_metric_keys()`（14 名）与 `heavy_metrics_complete()`（9 名）仅 6 名重合；`:347` 的 `active_window_seconds >= 0.0` 在默认 0.0 上**恒真** |
| F-M15 | `runtime_contract.h:267-270` | `release_lease` 过量释放把 `leased_total_` 归 0 ⇒ 把**全部**预算一次性还回（账目静默失真） |
| F-M16 | `command_tree.h:34,55` vs `parser.cpp:38-56` | 「唯一事实源」的两张旗标表**零消费者**，在用的是手抄孪生表 ⇒ `README.md:37-38` 与 `command_tree.h:3-5` 双双被证伪 |
| F-M17 | `memory_growth.h:37,104,116-117` | `MemDiag::Leak` 默认配置下**不可达**；任意正斜率一律 `Growing`；模块**零生产消费者**却带 4 个无出处魔法数 |
| F-M18 | `memory_report.h:611-617` | `validate_alloc_report` 用**同一个** `theil_sen_slope_xy` 复算**同一执行刚写出**的 JSON 里的同一个斜率 ⇒ 自洽式断言，对斜率定义缺陷零分辨力；且**零生产调用方** |
| F-M19 | `process.cpp:166-172`、`:87-92` | `devnull_stdio` 打开失败被静默跳过（fail-open）：子进程继承父 stdout，噪声污染 JSONL，而 `ok()` 仍返回 true |

### 4.3 建议（SUGGESTION，14 条）

| ID | 位置 | 问题 |
|---|---|---|
| F-S1 | `resource_gate.h:15-16,53` | 过期注释仍宣称「门禁失败 → exit 10」「→ 协作取消」，与同文件 `:4-6/:268-270` 及 `commands.cpp:990-991` 矛盾 |
| F-S2 | `resource_gate.h:233` | `>=` 与契约「**严格大于** 10 s」及常量名 `kMinActiveWindowSecondsExclusive` 矛盾（恰好 10.0 s 边界） |
| F-S3 | `resource_gate.h:105` vs `:207,336,363,368-370`；`memory_report.h:25` vs `:54,63,64,66`；`memory_growth.h:4` vs `:36,38,105` | 三处「唯一数值源/无硬编码散落」承诺与 13+ 个字面量阈值并存 |
| F-S4 | `resource_gate.h:343-347` | 单线程判据回退用 `selected_workers < 2 \|\| max_active_threads < 2`；契约 `min_active_compute_threads_fallback_statistic:"peak"` 规定只用 peak，且 `denominator.forbid[0]` 禁止以配置冒充观测 |
| F-S5 | `commands.cpp:1218` vs 契约 `:54-60` | 队列「有工作」谓词用 `runnable_workers > 0`；契约要求 `runnable_p50 > allocated_capacity_cores` 且 `min_runnable_threads: 2`，note 逐字点名「仅有 2 个线程在 16 核配额上跑是并行宽度不足，**不是 CPU 饥饿**」 |
| F-S6 | `commands.cpp:1221,971` | 队列窗口与 first-10s 边界用硬编码 `0.5` 采样周期，而 `ResRecord::elapsed_seconds` 真实单调时间戳就在手边；且 `ResourceRecorder` 默认间隔是 0.25（`resource_recorder.h:97`），非 `commands.cpp:922` 的构造点已 2× 错 |
| F-S7 | `resource_gate.h:519-520` | 诊断文案写 `>`，判据（与契约）用 `>=` |
| F-S8 | `commands.cpp:1315,1316` | `queue_depth_observed` 用低利用**连续时长**冒充队列深度；`worker_balance_active_over_runnable` 赋的是 worker 数 **p50**，而契约定义为 `active/(active+runnable)`（与 F-B1 同源，且由恒 50% 使该字段恒 0.5） |
| F-S9 | `acsd_process.h:29` vs `process.cpp:188-194` | 承诺「杀进程树」，实现只杀直接子进程（无进程组/Job Object）；唯一调用点是 `python3` 渲染器 |
| F-S10 | `cancel_token.h:32-34` + `runtime_client.cpp:505-507` | 无信号升级 + 无界 `join()` ⇒ Ctrl-C 不可杀挂死运行；无 SIGHUP |
| F-S11 | `session_commands.h:43,96,420-423` | `session_of` 未知名 → `SessionId(0)` → `input_contract` `default:` 静默给 EXPORT 合同；`render_checks` 未知 level 渲染成绿色 |
| F-S12 | `session_commands.h:224,267` vs `subcommand.h:109,116` | `--template` 产物（空 `hips_paths`、空 `hips_dir`）**被本模块自己的预检拒绝**；`:237,269` 还发 `"output_dir":"."`，与 `commands.cpp:657-667` 的「禁 CWD 残留」纪律相反 |
| F-S13 | `README.md:43`、`runtime_client.cpp:148`、`runtime_contract.h:174,185`、`session_commands.h:173/233/245/284`、`resource_recorder.h:284`、`cli_common.h:39` | **悬空引用 9 处**：`eng/ci/`（整目录不存在）、`runtime_mutation_driver.py`、`eng/tools/check_cli_command_layer.py`、`commands.cpp::cli_install_dir` |
| F-S14 | `resource_gate_thresholds_generated.h.in:10` + 根 `CMakeLists.txt:167` | 无 `CMAKE_CONFIGURE_DEPENDS` ⇒ 契约 JSON 改动不触发重新 configure；**陈旧生成物已实证**（`build/`、`build/linux-control/` 仍为 `namespace astrocs`） |

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 是否推翻 |
|---|---|---|---|
| R1 | 取任意合法运行，令 `eff = granted_workers`。由 `commands.cpp:960` `set_workers(eff, eff)` 与 `resource_recorder.h:368` `util = active*100/(active+runnable)`，代数化简得 `util ≡ 50.00` | 推翻「worker_balance.csv 的 utilization_pct 是实测不平衡指标」 | **推翻（恒等式成立）** |
| R2 | 构造磁盘 100% 满但仍有 1 个 4 KiB 块：4096 B 全进 stdio 缓冲 ⇒ `fwrite` 返回 4096 ⇒ `pr.err` 保持 0 ⇒ `classify_write_failure(0)=None` | 推翻「磁盘满 ⇒ exit 10」 | **推翻** |
| R3 | 构造 `MemAnalysisConfig cfg;`（默认 T=0.0）+ `rss={1000,1000,1000,1000,10000000}`：`:104` 恒假、`:116` 的 `T>0` 恒假 ⇒ 返回 `growing` | 推翻「泄漏会被判为 Leak」 | **推翻（10 MB 泄漏判为早期预警）** |
| R4 | 构造 `peak_rss = 20 MiB`、`last = 20 MiB`（回落比例 0）：`:186` `retained=0 ≤ 32 MiB` 恒真 ⇒ `Reclaimed` | 推翻「回落不足会判 UnexplainedResidual」 | **推翻** |
| R5 | 构造 16 worker 配额、租约从 1 线性爬到 16 且每个被授予核用满：均值 8.5 < 阈值 `0.85*peak16=13.6` ⇒ `LowAvgCores` | 推翻「正确实现必绿」 | **推翻（零缺陷 run 被判违规）** |
| R6 | 构造 16 核配额、仅 2 个 runnable 线程、CPU 利用率 < 60% 持续 10 s：契约 note 明说这不是 CPU 饥饿，实现 `runnable>0` 成立 ⇒ `QueueStarvedCpu` | 推翻「队列判据符合契约」 | **推翻** |
| R7 | 构造 active 窗口恰好 10.0 s：契约「严格大于 10 s ⇒ NotApplicable」vs 代码 `>=` 进入统计判据 | 推翻适用域边界一致 | **推翻** |
| R8 | 把 `theil_sen_slope_xy:341` 的 `return slopes[mid]` 改成 `return slopes[0]`（或把 `:481` 的 `/1048576.0` 改成 `/1024.0`），重跑 `validate_alloc_report` | 推翻「自校验能发现斜率定义缺陷」 | **推翻（同函数同源，无分辨力）** |
| R9 | 让 `gen_run_graphs.py` 运行在拒绝 `open("/dev/null")` 的沙箱：`:167` 的 `if(devnull>=0)` 不成立即跳过，渲染器继承父 stdout | 推翻「JSONL 通道恒纯净」 | **推翻** |
| R10 | 构造 `active_window == 10.0`，且 avg 恰好 8.5 / 16 worker | 同时命中 R5 与 R7，说明二者叠加使边界点既非豁免也非正确判定 | **成立（两缺陷正交）** |

---

## 6. 盲复算（遮住既有判定独立取证）

方法：先写下本片结论，**再**单独对每条既有判定重取证据，不引用前序推理；结论一致性逐条标注 `一致 / 偏松 / 偏严`。

| 既有判定出处 | 我独立取证的动作 | 结论 |
|---|---|---|
| 前序审稿对 `lib/` 的整体「门为恒真不可信」印象 | 我在 `resource_gate.h:268` 读到两个形参被注释掉的 `gate_enforcement`；再在 `resource_gate_v1.json:74-80` 读到 `in_process_hard_fail:"retired"` | **一致**（record-only 是契约，不是缺陷；缺陷在注释与诊断字段的自证性） |
| 「磁盘门是唯一硬门且已 fail-closed」 | 我逐行读 `probe_writable` 并对照 `resource_recorder.h:310`、`memory_report.h:282` 已修的同款 | **偏松**（既有判定认为已闭合；实际 `disk_gate.h` 是唯一漏改处） |
| 「worker 观测已按 B2-A18 改为真实租约观测」 | 我从 `commands.cpp:960` 反推 `set_workers(eff,eff)` 到 `resource_recorder.h:368` | **偏松**（真实租约值确实进了 `eff`，但同一值被同时当作 active 与 runnable，使下游平衡指标恒 50%） |
| 「异常实验域归档已清理」 | 我 grep 到 **229 个无 `.py` 的 `.pyc`**，含判别力/同义反复检查器 | **偏松**（`.py` 已清，但 `.pyc` 残留且正是被引为「注入锚点引擎」的那一个） |
| 「内存泄漏检测已接线」 | 我对 `analyze_memory_growth` 全仓 grep 生产消费者 = 0 | **偏严→实为偏松**（既有印象以为有接线；实为死模块，且即便接上默认配置也判不出 Leak） |
| `G08-05-修复-lib路径与模块计数.md` 的模块计数 22 | 我跑了该报告的原始命令 `grep -oE 'module_id = "[^"]+"' module_adapters.cpp | sort -u | wc -l` | **一致（无法证伪）**；但我另发现 CLI 的 `acsd.infrastructure.cli` 在该 22 之外、且全仓仅 2 行 ⇒ 该计数对本片无覆盖力 |
| 「`--version` 等命令面已收敛到 command_tree 唯一表」 | 我 grep 到 `boolean_flags`/`value_flags` 零消费者、`parser.cpp` 手抄孪生表 | **偏松** |

---

## 7. 子代理派发记录

**派出 5 个**（去重后 **3 个不同任务**；因工具层重复提交，任务 A、B 各被派发两次，产物一致）。全部只读、零 git 写、未编译、未跑二进制。

| 任务 | 覆盖文件 | 核心产出 |
|---|---|---|
| A（×2 重复） | `resource_gate.h`、`.h.in`、`disk_gate.h`、`memory_report.h`、`memory_growth.h`、`resource_recorder.h`、`resource_events.h`、`mode_gate.h` | 4 阻断 + 16 须修 + 11 建议；独立提出 `worker_balance` 恒 50、`probe_writable` 丢 `fclose`、`§6.3` 伪引 |
| B（×2 重复） | `commands.cpp`、`process.cpp`、`main.cpp`、`acsd_process.h`、`cancel_token.h`、`subcommand.h`、`command_tree.h`、`protocol.h`、`runtime_client.{h,cpp}`、`runtime_contract.h`、`session_commands.h` | 4 阻断 + 21 须修 + 20 建议；独立提出私建池、§10.4 零写者、devnull fail-open、flush 时序 |

**注**：第三个子代理（任务 C：`parser.cpp`/`jsonl.h`/`cli_common.h`/`profile_store_bridge.h`/`monitor.h`/`exit_codes.h`/3 子命令头 + README/module.yaml/CMakeLists/`.h.in`）虽已提交成果，但其结论我**未在正文中单列**，因其中关于 `pss_bytes`/`read_proc_self`/JSONL 4099 字节的部分已由任务 A/B 独立或由我本人复现并采信，避免重复计数。

### 逐条复核与**否决**记录

我**亲自**重取证据复核了子代理的每一条阻断/高危须修。采信 6 条、**否决或降级 5 条**：

| 子代理结论 | 我的复核 | 处置 |
|---|---|---|
| A: `worker_balance.utilization_pct` 恒 50.00 | 我读 `resource_recorder.h:368-369` + `commands.cpp:941,960`，手工代数化简 ⇒ 恒等 | **采信 → F-B1**（我的首要阻断） |
| A: `probe_writable` 丢 `fclose` | 我读 `disk_gate.h:231-239` 全段，确认 `fwrite` 4096B 进缓冲、`:236` 返回值丢弃、`:199` `case 0 → None` | **采信 → F-B2** |
| A: `§6.3` 是「投影算法」伪引 | 我 `sed -n '381,390p' docs/ACSD_DESIGN.md` 亲见 `### 6.3 投影算法` 与 TAN/SIN/CAR… | **采信 → F-M1** |
| B: 私建池 + §10.4 零写者 | 我读 `commands.cpp:1506-1519` 确认池；`register_source/request_lease` 的「零生产调用者」我未亲自重跑全仓 grep，**标为子代理单点证据**，但池本身我亲见 | **部分采信 → F-B3**，其中「零写者」一项降级为待前台复核 |
| B: `process.cpp` devnull 静默失败 | 我读 `process.cpp:166-172` 确认 `if(devnull>=0)` 无 else | **采信 → F-M19** |
| C: `pss_bytes` 恒 0 | 我读 `monitor.h` 的 `read_proc_self` 全段，确认只开 status/io/stat + sysinfo，从不读 smaps_rollup | **采信 → F-M3** |
| A/B: 「资源门判据在正确实现下也报红（均值 vs 峰值分母）」 | 我独立算：峰值 16 / 均值 8.5 / 阈值 `0.85*16=13.6` ⇒ 判 `LowAvgCores`。**但** `gate_enforcement` 恒 RecordOnly，故该「红」只进事件不改退出码 | **降级**：由阻断降为须修 F-S4（语义偏差而非 fail-open），并在正文如实标注 record-only 前提 |
| B: `eng/ci/` 整目录不存在 → 两道「机器断言」不跑 | 我 `ls eng/ci` 亲自确认不存在 | **采信 → F-S13** |
| B: `cmd_verify` 缺 `is_object()` 守卫会抛异常 → 70 | 我读 `commands.cpp:2454-2472`，未见 try/catch；但**无法在只读+不编译前提下证实 nlohmann `value()` 的确切抛点** | **降级为待验**（未列入正式清单，见 §8） |
| C: G08-05 模块计数 22 判定「无法证伪」 | 我跑了原始命令得 22，并见 `phase1/2/3.node` 是 kind 标签的陷阱 | **一致 → §6** |
| A: 229 个孤儿 `.pyc` | 我亲自跑计数脚本：orphan 229 / total 229，并 `ls` 确认 `check_taut_null_guard`/`check_test_discriminative`/`runtime_mutation_driver` 三个 `.pyc` 在、`.py` 不在 | **采信 → F-B6**（提升为阻断，因它是「判据不可信」这一裁决的直接成因） |

**另有一条我否决子代理的框架性结论**：任务 A 主张「本组 8 文件没有一个是改变退出码的门」，易被读成「本片无阻断项」。我按契约 `in_process_hard_fail:"retired"` 复核后确认该结论对**退出码**成立，但**不豁免** F-B2（磁盘门确有 exit 10 路径且 fail-open）与 F-B1（恒真产物直接进交付工件）。故未采纳其「无阻断」隐含推论。

---

## 8. 未决 / 交前台

| 项 | 说明 |
|---|---|
| U1 | `cmd_verify`（`commands.cpp:2454-2472`）与恢复哈希扫描（`:1731-1736`）对 `artifacts` 元素无 `is_object()`/`is_array()` 守卫。读侧代码**允许**元素是裸字符串（`runtime_client.cpp:574-576`、`commands.cpp:2272` 都有 `if(!a.is_string()) continue;`），而 verify 侧直接 `a.value("path", ...)`。是否真抛 `type_error` 需编译期确认（我只读，不编译）。 |
| U2 | `ProcessBudgetRegistry::register_source` / `request_lease` 的「零生产调用者」目前是子代理 grep 证据，我未亲自全仓复跑。前台若能跑 `grep -rn "register_source\|request_lease" lib/ --include=*.cpp` 即可定案（预期：仅测试与 `budget.py`）。 |
| U3 | `commands.cpp:1718` 的恢复预检对**任一**畸形 `acsd_run_*.json` 置 `mismatch=true` ⇒ 该 output_dir 永久 rc=8、不可恢复（manifest 从不清理）。我判定为过宽的 fail-closed，但未构造可执行复现（禁止编译/运行）。建议前台按此方向注入。 |
| U4 | `main.cpp:72` 崩溃路径二次 `make_run_id()` 导致 run_id 不一致 —— 影响面取决于 GUI 是否按 run_id 关联事件与 manifest，仓内无消费者证据。 |
| U5 | HEAD 与派单书所写 `850a9ede` 不符（实际 `1fa477a7`）。若权威基线确为 `850a9ede`，本片某些行号可能有偏移，需重跑。 |

---

## 9. 自证段（可复跑命令）

```bash
# 0) 基线自证：HEAD 与派单书不符
cd "/workspace/Astro CS Database" && git -c core.quotepath=false log --oneline -1

# 1) 覆盖率自证：33 份 / 9588 行（与权威清单逐份相等）
cd "/workspace/Astro CS Database"
awk '/- 片号: INF-cli-001/,/- 片号: INF-gaia_xpsd_client-001/' \
  run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | grep -oE '"lib/infrastructure/cli/[^"]+"' | tr -d '"' > /tmp/sid.txt
wc -l < /tmp/sid.txt                      # 33
xargs wc -l < /tmp/sid.txt | tail -1      # 9588 total

# 2) F-B1 恒真产物（代数自证，无需运行）
grep -n "util = denom > 0" lib/infrastructure/cli/resource_recorder.h   # :369
grep -n "set_workers(" lib/infrastructure/cli/commands.cpp                # :941 与 :960，两处同值两传

# 3) F-B2 唯一硬门 fail-open
sed -n '225,240p' lib/infrastructure/cli/disk_gate.h                      # fclose 返回值丢弃
grep -n "case 0: return WriteFailureKind::None" lib/infrastructure/cli/disk_gate.h

# 4) F-M1 §6.3 伪引（实为投影算法）
sed -n '383,387p' docs/ACSD_DESIGN.md
grep -n "| 10 |" docs/engineering/ERROR_HANDLING_STANDARD.md

# 5) F-M3 pss_bytes 恒 0（read_proc_self 从不读 smaps_rollup）
grep -n "fopen(\"/proc" lib/infrastructure/cli/monitor.h

# 6) F-M4 read_proc_self 恒真
grep -n "(void)ok;" lib/infrastructure/cli/monitor.h

# 7) F-M8 4096 上限可被突破
grep -n "kEventLineMaxBytes - 1\|return cut + \"…\"" lib/infrastructure/cli/jsonl.h
grep -rn "kEventLineMaxBytes\|clip_utf8\|fit_event_line" lib/ eng/ | grep -v "jsonl.h:"   # 预期无测试命中

# 8) F-M9 EDQUOT/EFBIG 漏映射
grep -n "e == ENOSPC" lib/infrastructure/cli/jsonl.h
grep -n "ENOSPC / EDQUOT / EFBIG" eng/contracts/resource_gate_v1.json

# 9) F-M14 两份字段清单不一致
grep -n "required_metric_keys\|heavy_metrics_complete" lib/infrastructure/cli/runtime_contract.h

# 10) F-M16 唯一事实源被证伪
grep -rn "boolean_flags\|value_flags" lib/ eng/          # 仅定义 + 注释
grep -n "cmd_boolean_tokens\|cmd_value_tokens" lib/infrastructure/cli/parser.cpp

# 11) F-M17 Leak 不可达 + 死模块
grep -n "leak_slope_bytes_per_iter" lib/infrastructure/cli/memory_growth.h
grep -rn "analyze_memory_growth\|memory_growth.h" lib/ --include=*.cpp --include=*.h

# 12) F-M18 自洽式断言
grep -n "theil_sen_slope_xy" lib/infrastructure/cli/memory_report.h     # :480 写、:615 复算，同一函数
grep -rn "validate_alloc_report" lib/ eng/                              # 预期无生产命中

# 13) F-B6 孤儿 .pyc（判别力检查器已删）
cd "/workspace/Astro CS Database"
n=0; for f in $(find eng lib docs -name "*.pyc"); do
  [ -f "$(dirname "$f")/$(basename "$f" | cut -d. -f1).py" ] || n=$((n+1)); done; echo "orphan .pyc = $n"
ls eng/tools/quality/__pycache__/ | grep -E "taut_null|test_discriminative"

# 14) F-S13 悬空引用
ls eng/ci 2>&1 | head -1
ls eng/tools/check_cli_command_layer.py 2>&1 | head -1
ls eng/tools/quality/runtime_mutation_driver.py 2>&1 | head -1
grep -n "commands.cpp cli_install_dir 共用" lib/infrastructure/cli/cli_common.h

# 15) F-S14 陈旧生成物（无 CMAKE_CONFIGURE_DEPENDS）
grep -rn "CMAKE_CONFIGURE_DEPENDS" --include=CMakeLists.txt --include=*.cmake . | grep -v "^./run/"   # 预期零命中
for f in $(find build -name resource_gate_thresholds_generated.h 2>/dev/null); do
  printf "%s: " "$f"; grep -m1 "^namespace" "$f"; done

# 16) F-M12 退出码字面量
grep -n "return 70;\|return 10;\|return 3;\|return 2;\|return 0;" lib/infrastructure/cli/runtime_client.cpp

# 17) 模块计数（我复核过、与 G08-05 一致）
grep -oE 'module_id = "[^"]+"' lib/infrastructure/scheduler/src/module_adapters.cpp | sort -u | wc -l   # 22
grep -rn "acsd.infrastructure.cli" lib/ eng/ docs/   # 预期仅 module.yaml 2 行
```

**注**：上述命令全部为只读（`ls`/`grep`/`sed -n`/`find`/`awk`/`wc`）；`/tmp/sid.txt` 为仓外临时文件，未写入仓内任何路径。