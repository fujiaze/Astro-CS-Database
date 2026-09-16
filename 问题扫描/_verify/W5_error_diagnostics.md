# W5 轴 · 错误处理与诊断完备性（第二轮全仓静态审查）

- 轴：W5「错误处理与诊断完备性」
- 时点：HEAD `a3a343a44080d917089e1f8d548ed2d0400b0c61`（`git --no-optional-locks rev-parse HEAD`）
- 方式：纯静态（read/glob/grep + 只在 `问题扫描/` 内跑只读 `python3 -B` + `git --no-optional-locks` 只读子命令）；未跑编译器/cmake/ctest/测试/二进制/`ci/run.py`/Fatduck/任何 git 写操作
- 准绳顺序：①负责人设计意图（宪章 §18 裁决、README/合同内明示的"有意为之"）②现行冻结文本（`ASTROCS_PROJECT_CONSTITUTION.md` = ASTROCS-CONSTITUTION-001）③代码测试自洽
- 免重报依据（已读）：`问题扫描/SUMMARY.md` 14 机制簇表、`问题扫描/INDEX.md §三` 唯一总述表、`问题扫描/账本/FIX_LEDGER.csv`（682 行，逐锚点关键词比对，脚本与命令见 §九）
- 可复算脚本（只读，全部在本目录）：`scripts_w5_census_raw.py`（权威普查，输出 `scripts_w5_census_raw.out`）、`scripts_w5_census_final.py`、`scripts_w5_sweep1..4.py`、`scripts_w5_corpus.py`、`scripts_w5_ledger_terms.py`、`scripts_w5_ledger_detail.py`、`scripts_w5_dedup.py`、`scripts_w5_dedup2.py`、`scripts_w5_dedup3.py`、`W5_excluded_files.txt`
- 条目数：**15**（W5-N-01…W5-N-15，宁窄而实）

---

## 〇、口径（每个裸数字都挂：口径 + 时点 + 复算命令）

| 口径 ID | 定义 | 数值 |
|---|---|---|
| W5-A 语料 | `git --no-optional-locks ls-files` 中，生产 C/C++ = 前缀 `lib/ cli/ include/ runtime/ providers/ modules/` 且后缀 `.cpp .h .hpp .c` | 850 |
| W5-B 核心 | W5-A 再排除 vendored 子串 `/archive/ /legacy/ nanoflann /browser_qt/ third_party` | **706**（其中 `lib/acr/**` 164、非 ACR 542） |
| W5-B2 门面 | `tools/ ci/ scripts/` 下 `.py` | 162 |
| W5-B3 测试 | `tests/` 下 C/C++ | 148 |
| W5-C 吞错普查 | 在 W5-B 的 706 文件上，**原始文本**定位 `catch(...)`（花括号配平取正文），正文分类只看**等长掩码串**（注释→空格、字符串字面量内容→点），故行号 = 原始行号 | catch 总数 **491** |

排除清单全量 144 个文件见 §十一（分组计数：cfitsio 105、healpix_stack(archive/legacy) 29、json-schema-validator 9、nanoflann 1）。

复算：`python3 -B 问题扫描/_verify/scripts_w5_census_raw.py`

交叉核对（口径差异如实记录，不取均值）：`scripts_w5_sweep4.py` 先整体剥注释再数 catch，得 total=503 / 空体=35 / 只记录=46 / 其它=30；权威普查（W5-C）得 491 / 35 / 41 / —。差 12 站点，成因是剥注释方式对 `}` 前 catch 与宏内 catch 的可见性不同；**空体数 35 在两口径下一致**。本报告一律采 W5-C，且所有已发布锚点都用原始文本 `grep -n` 复核过（sweep4 的行号因剥块注释会整体前移，不可作锚点——例如 cpu_routing 同一站点 raw=412 / strip=401）。

---

## 一、吞错点分类计数（W5-C 口径，时点 a3a343a4）

| 类 | 判据（掩码后正文） | 总数(core 706) | 非 ACR | ACR |
|---|---|---|---|---|
| E0 空 catch / 注释-only catch | 正文为空 | **35** | **21** | 14 |
| E2 catch 只记录、不改控制流也不改状态 | 无 return/throw/break/continue/goto/赋值/计数/`fail(` | **41** | 37 | 4 |
| P 有处置（传播/置失败/记状态/返回码） | 命中上述任一 | 415 | 388 | 27 |
| X catch 后非 `{` | — | 0 | 0 | 0 |

E0 非 ACR 全量 21 处锚点（逐条已读；其中构成缺陷的才进 §二）：

```
cli/commands.cpp:556   cli/commands.cpp:1139  cli/commands.cpp:1184
cli/commands.cpp:1214  cli/commands.cpp:1367  cli/runtime_client.cpp:389
lib/astro_image_io/src/aio_fits.cpp:161        lib/astro_image_io/src/aio_fits.cpp:162
lib/backend_host/cpu_routing.cpp:412           lib/backend_host/cpu_routing.cpp:589
lib/backend_host/worker_advisor.cpp:72         lib/core/src/executor.cpp:137
lib/core/src/executor.cpp:254                  lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1149
lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:2275
lib/orchestrator/cpp/src/main.cpp:157          lib/phase2/tools/stage2.cpp:1775
lib/phase2/tools/stage2.cpp:1779               lib/plate_solve/cpp/ipv/src/ipv_entry.cpp:334
lib/plate_solve/cpp/ipv/src/ipv_entry.cpp:347  lib/plate_solve/cpp/ipv/src/ipv_entry.cpp:360
```

其余吞错形态（同一普查）：

| 形态 | 判据 | 计数 | 备注 |
|---|---|---|---|
| 门面 Python `except` | `tools/ ci/ scripts/` 162 文件 | 298 | — |
| └ 体仅 `pass`/`continue` | 缩进块内只有 pass/continue | **64** | 见 W5-N-12 |
| └ 裸 `except:` | 无异常类型 | **3** | `tools/quality/contracts/check_doc_symbols.py:51`、`check_full_integration.py:34`、`generate_contract_report.py:45`（连 `SystemExit`/`KeyboardInterrupt` 一起吞） |
| `(void)` 显式丢弃 | 非 ACR 生产行含 `(void)` | 303 行 | 绝大多数是 `(void)unused_param;` 未用参数抑制；与错误处置相关的 1 处：`lib/hips/src/aio_publish.cpp:382` `(void)fsync(fd);` |
| 副作用返回值丢弃（逐条看码后保留） | `create_directories/rename/remove/_mkdir/mkdir/CloseHandle/fsync` 语句位 | 4 组 / 12 站点 | `cli/commands.cpp:443,483,486`、`lib/astro_image_io/src/hips/aio_hips_writer.cpp:124,126`、`lib/astro_image_io/src/hiss_stream_writer.cpp:119,122,642,685,688`、`lib/hips/src/aio_publish.cpp:382`、`cli/monitor.h:124` |
| `[[nodiscard]]` | 生产 C/C++（去 vendored） | **0** | `grep -rnF '[[nodiscard]]'` 命中 0 行 |
| `std::ignore` | 同上 | 0 | — |
| `assert()` | 非 ACR 生产（排除 `/tests/`） | 0 | 仅 ACR 有 2 处：`lib/acr/scheduler/shared_work_pool.cpp:181,189` |
| retry/attempt/backoff 语义行 | 掩码后按行匹配 | 61 | **ACR 59**；非 ACR 仅 `lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp:476,494`；另 `cli/process.cpp:177` EINTR 单发重试 |
| 错误码字面量定义行（按前缀） | `include/` + W5-B | ACS_ERR_=60、AIO_ERR_=32、ACS_LOADER_EC_=20、AIO_PUBLISH_ERR_=16、ACS_REG_EC_=16、ACS_DIAG_ECODE_=16、ACS_FIO_ERR_=13、P3_RS_=4 | 同名前缀在 legacy/abi 两族头各计一次 → 见 §三 |

**判为"合理吞错"（不计条目，作判别证据）**：
1. `lib/phase2/tools/stage2.cpp:1775/1779`：顶层 handler 已 `fprintf(stderr)` + `return 1`，被吞的是**日志二次失败**（`try { log(...); log_flush(); } catch (...) {}`），吞错点在观测面不在失败面。
2. `lib/orchestrator/cpp/src/main.cpp:157`：inspect 打印路径解析失败退化为"输出原始 JSON"，非产品写路径。
3. `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp:334/347/360`：C 边界 setter/destroy 的异常屏障（注释自证"防止泄漏到 C 边界"），且 334 已在账本 `ipv_entry.cpp:333` 锚点覆盖范围内（V2-N-01/M1a-E-001/M6a-D-009），不重报。
4. `lib/astro_image_io/src/aio_api.cpp:414`：`aio_free_image_data` 是 `void` 返回型 C 边界屏障，异常**已记录**（消息含符号名 + "ignored"），设计上有意的取舍。
5. `cli/commands.cpp:685`：CPU 画像失败退 baseline，宪章 §10.3 明文允许"无或损坏 profile 时退 baseline"——退 baseline 本身合规（本轴只在"该决策不留任何记录"处判缺陷，见 W5-N-02）。

---

## 二、最危险的 15 处（W5-N-01…W5-N-15）

### W5-N-01 · P1 · executor 把抛异常的任务记成 COMPLETED（观测面伪成功）
- 位置：`lib/core/src/executor.cpp:137`（CpuHeavyExecutor worker 循环）、`:254`（IoExecutor 同形）
- 事实：`try { task(); } catch (...) {}` 之后照常 `tasks_executed_.fetch_add(1)`，并在 `:151` 写 TraceEvent `e.status = "COMPLETED"`；`TraceEvent` 本身有 `status/error/error_domain` 三字段（`include/astrocs/core/contracts.h:213-215`），此处 error 留空。
- 后果：observed_trace.json（`cli/commands.cpp:567`）、RUN_GRAPH 渲染（`tools/quality/gen_run_graphs.py:48` 把 `COMPLETED` 映射为绿色）、CHK-002 静态/观测双向比较，三条链都吃进"抛异常 = 成功"。同仓对照：`lib/core/src/runtime.cpp:245` 用 `r.failed() ? "FAILED" : "COMPLETED"`，`lib/core/src/scheduler.cpp:285` catch 后 `node_ok=false; record_failure(...)` —— 同一异常在调度层有处置，在 executor 层被吞并涂绿。
- 命中频率：需运行期，未判（是否有任务在 executor 层抛出，取决于任务体是否自带 catch）。
- related：M5a-G-005（同文件）、SUMMARY 簇「吞错后走空返回」。

### W5-N-02 · P1 · profile 解析失败处注释自称 handled，实际静默改 baseline
- 位置：`lib/backend_host/cpu_routing.cpp:412` `try { prof = nlohmann::json::parse(profile_json); } catch (...) { /* handled */ }`；同文件 `:589` `} catch (...) { /* empty */ }`
- 事实：注释声明"handled"，掩码后正文为空；`prof` 保持空对象，后续 `kernel_row(prof, ...)` 取不到 kernel 行 → `want_provider` 落字面量 `"baseline"`（同文件 `:416` `jget_ref<std::string>(*kp,"provider","baseline")`）。
- 判别：退 baseline 本身符合 §10.3（负责人意图），**缺陷在处置不留痕**：同族条件下 `lib/backend_host/worker_advisor.cpp:208` 记 `chain += "profile_unreadable|"`、`lib/backend_host/profile_store.cpp:363/390` 显式置空并回落 —— 同仓对同一条件两种处置，且本处的注释与实际行为相反。§14.4 明禁"静默改默认值"。
- related：M5a-G-010（同文件）。

### W5-N-03 · P1 · cgroup 限额解析失败按"无上限"处理（fail-open）
- 位置：`lib/backend_host/worker_advisor.cpp:72` `} catch (...) { return false; }`（cgroup 字符串解析助手）
- 事实：解析抛错与"键不存在"共用同一出口 `false`，调用方据此按"无 cgroup 约束"计算 workers 上限；同文件 `:146`（字段类型篡改）与 `:208`（profile 不可读）都显式记录回落，`:72` 既不记录也不区分。
- 后果：容器/cgroup 文本形态稍有出入即失去 OS 约束，直接抬升 §10.3"据 affinity/cgroup 动态定并行度"的 worker 数（§10.5 门禁的分配容量口径随之失真）。
- 触发面：需运行期，未判（取决于 sysfs/cgroup 文本形态）。
- related：M5a-G-006、V7-N-08（同文件）。

### W5-N-04 · P1 · 退出码归因链上的吞错与跨节点归因
- 位置：`cli/runtime_client.cpp:389`（`} catch (...) {}`）、`:416`（`catch (...) { continue; }`）
- 事实：`exit_code_from_error` 判定"是否 INPUT(3)"的方式是遍历**本次 run 全部** `g_manifests`，任一 manifest 的 `error_kind=="input"` 即整次退 3；manifest 不可解析 → 该节点直接不参与判定（空 catch），落回 `DATA→2 / IO→7`。
- 两面：(a) 解析失败使归因静默失败（吞错，影响退出码与 JSONL final 事件）；(b) 归因主体是"任一节点"而非"失败节点"，多节点 run 存在串扰；其上方 `:382-384` 注释自述为"失败 manifest 的 error_kind==input → 3"，实现与自述不一致（准绳③）。
- related：M5b-C-05（该条只覆盖 `RESOURCE→5` 与三处码表互不覆盖，本条不重报该面）。

### W5-N-05 · P1 · 配置文本解析失败 → 产物落点静默改成进程当前目录（6 处同构）
- 位置：`cli/commands.cpp:867`、`:1051`、`:1149`、`:1377`、`:1727`（同一个 lambda：`try { json::parse(cfg_text).value("output_dir",".") } catch (...) { return "."; }`），以及 `:1214`（`prior_cfg_doc = parse(cfg_text)` 吞错后 `scan_dir` 退 `"."`）
- 事实：run manifest、resource_samples.csv/resource_summary.json/worker_balance.csv、provenance、graph 产物的写出目录都由这段兜底决定；失败时产品写到**当前工作目录**，run 仍返回 0，无 warning 事件、无 stderr。
- §14.4 判定：同一 `cfg_text` 已在 `:1071/:1204/:1659` 过 `validate_config_full` 信任边界，此处属"边界之后重复解析同一文本并各自就地默认"，同时命中"静默改默认值"与"同一条件重复校验"两条禁令。
- related：V7-N-03（缺键→就地默认 机制级；本条差异：默认值来自 catch 而非缺键，且落点是产物写出目录）、M5b-C-06（build_run_provenance）。

### W5-N-06 · P1 · 忽略 `ok` 出参：空串冒充 sha256 进入 manifest/sidecar/trace
- 位置（21 个 `file_sha256` 调用点中，`ok` 声明后从不读取的 8 处）：`cli/commands.cpp:120`、`:138`、`:169`、`:466`、`:542`、`:553`、`:579`、`:1884`；读了 `ok` 的对照面 = `:1070-1071`、`:1203-1204`、`:1257-1258`、`:1433-1435`、`:1658-1659`、`:1822-1823`、`:1841-1842`（这些点 `if (!ok)` 直接退 INPUT/mismatch，处置正确）
- 事实：`cli/parser.cpp:238 file_sha256(path, bool* ok)` 读失败时返回空串并置 `*ok=false`；上述 8 处把空串直写 JSON 字段 `sha256`/`ir_sha256`/`output_artifacts[].sha256`。同族的另一写法在读了 `ok` 的点上也违约：`:1339`、`:1342`、`:1351`、`:1718` 用 `{"sha256", ok2 ? sha : ""}` —— 失败时写**空串**而不是 `null`，故"检查了 ok"并未换来合法形状
- 后果：`contracts/schemas/jsonl_event_v1.schema.json` 的 `/properties/sha256` = `{"type": ["string","null"], "pattern": "^[0-9a-f]{64}$"}`（已用解释器解析读出，非肉眼判读）—— 空串既非 null 也不合 pattern；run manifest 的 artifact 事件与 graph_sidecar/observed_trace 的哈希字段变成"看起来像值、实为无值"，verify/provenance 面无从区分"没算"与"算错"
- 是否触发：需运行期（要求清单写盘时该文件不可读），静态可判的是**形状违约且无兜底**。
- related：M8a-G-010（同函数）、M6a-G-002（create_directories）、M2b-C-01（fsync 同族丢弃）。

### W5-N-07 · P1 · 跨语言错误码名值表：`acs_status_name_v1` 声明无定义
- 位置：`include/astrocs/abi/status_codes.h:210-211`（`const char* acs_status_name_v1(acs_status);` / `acs_status_domain_name_v1(int32_t)`），头内注释承诺"稳定枚举 → 冻结字符串名（与 manifest/schema enum 对齐; 非法值返回 NULL）"
- 三级复核（命令与输出见 §九）：全仓（排除 `run/` 影子树与 `问题扫描/`）**只有 2 处声明 + 1 处注释引用，0 处定义**。
- 同族对照：`acs_artifact_status_name_v1` 有生产定义（`runtime/artifact_store/artifact_abi_v1.c:624`）；`acs_lc_state_name_v1`/`acs_module_op_name_v1`（`include/astrocs/abi/lifecycle_v1.h:237-238`）只在测试探针里由测试自己实现（`tests/abi/abi002_lifecycle_probe.c:101/117`）→ 同一"名值表"家族三处一致性不齐：status 无实现、lifecycle 仅测试实现、artifact 生产实现。
- 后果：任何生产 TU 采用该声明即链接失败（静态判）；实际诊断面被迫自造表（见 W5-N-08）；Python 门面侧没有任何 `ACS_ERR_*`/`AIO_ERR_*`/`detail_code` 名值映射。
- related：M5b-E-04（acs_status 重复定义）。

### W5-N-08 · P1 · 宿主私有名值表漏最新枚举，值 10 在宿主侧退化为 UNKNOWN
- 位置：`lib/core/src/module_adapters.cpp:300-315 status_str(acs_status)`（case 覆盖 0,1,2,3,4,5,6,7,8,9,70）、`:317-329 to_result()`
- 事实：switch **无 `ACS_ERR_EXCEPTION = 10` 分支** → `status_str(10)` 落 default 打 `"UNKNOWN(10)"`；`to_result(10, what)` 无 case → 落 default `ErrorDomain::INTERNAL`。产生侧真实存在：`status_codes.h:164` 定义 10="DLL 边界捕获内部 C++ 异常并转换（禁异常外泄）"，`lib/calibration/src/module_entry.cpp`、`lib/cosmetic/src/module_entry.cpp` 等模块入口按该族头使用它；而宿主 TU 只 include legacy `astrocs/common_abi_v1.h`（`module_adapters.cpp:41`，该族无值 10）→ 同名枚举、两套成员，语义由 TU 选哪份头决定。
- 同一整数的第二处折叠：`ACS_ERR_STATE=7`（生命周期违例）与 `ACS_ERR_SELFTEST=9`（"加载后 self-test 失败, 不得运行"）同样落 default INTERNAL → 经 `cli/runtime_client.cpp:395-400` 变 CLI 70；而 `cli/exit_codes.h` 里"签名/加载/自检失败"属 BACKEND(5)。SELFTEST 的返回点真实存在：`lib/backend_host/backend_table.inc:50`。
- 相关可达性：宿主是否会收到值 10 取决于模块化 ABI 模块在 CLI 路径上的实际调用面 → 需运行期，未判；静态可判的是**表缺项 + 域折叠 + 两族枚举同名不同集**。
- related：M5b-E-04、M5b-G-13（aio_abi_v1）、M5b-C-05。

### W5-N-09 · P1 · 被当作权威的错误码登记表文件不存在
- 引用方：`lib/orchestrator/cpp/include/orchestrator.h:111`、`tools/gen_audit_pack.py:74` 都把 `engineering/contracts/error_code_registry.csv` 指为错误码登记事实源
- 三级复核（命令与输出见 §九）：该路径**不存在**（`git ls-files | grep` rc=1；`engineering/contracts/` 下无此文件；仅 `设计大纲/_evidence/packs/history/` 内有历史副本）
- 后果：`AstroCsExitCode`（`orchestrator.h`：0-10、20-28、100 基址）与 CLI 11 码段之间的分配表无落库载体 → "码 → 含义 → troubleshooting 条目"链断（`docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md:9` 要求该链可定位）
- 免重报核对：账本内 `error_code_registry` 关键词 0 命中。

### W5-N-10 · P2 · 越界退出码 124/127 + "唯一源"门只扫 `cli/`
- 位置：`cli/commands.cpp:306` `rc = 124;`（子进程超时，注释"与原 timeout 命令超时退出码保持一致"）、`:309` `rc = 127;`（spawn 失败）
- 事实：冻结码集 `{0,2,3,4,5,6,7,8,9,10,70}`（`cli/exit_codes.h`；同一集合写进 `contracts/schemas/jsonl_event_v1.schema.json` 的 `exit_code` enum）不含 124/127；二者是 shell/`timeout` 惯例值，被 `cmd_test_synthetic` 当子进程 rc 再映射使用。是否会被 `emit_final` 当 exit_code 发出 → 需运行期，未判。
- 第二份数值表：`include/astrocs/core/contracts.h:41-46` 的 `enum class ExitCode : uint8_t { OK=0, ARGS=2, ... }` 重列了全部字面量，其行内注释自称"cli/exit_codes.h 唯一源; 此处只做映射表, 不重定义数值"，与 `cli/exit_codes.h:2`"其他文件只 include, 不得重定义数值表"直接冲突；`astrocs::core::ExitCode` 在生产无消费者（grep 仅定义处）。
- 门盲区：`tests/cli/test_cli_protocol.py::test_10_exit_codes_single_source` 的扫描范围是 `for fn in os.listdir(CLI)`（只 `cli/`），`include/` 下的重复表与 124/127 都在判据之外。
- 另一单向面：`tools/check_api_docs.py` 的 `check_exit_codes` 用 checker 自带 `EXIT_NAMES` 过滤头文件字面量（`if int(d) in EXIT_NAMES`），只做 doc→头 单向检查；头里新增/改名而不命中该表的值不会报。
- related：M5b-C-05、M5b-E-03、V18-N-05（判据来自被检物自身的机制级）。

### W5-N-11 · P2 · 生产诊断无坐标承载，状态返回值无编译期约束
- 准绳：`docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md:9`"错误必须可定位：error code → troubleshooting 条目 → source/test"
- 事实（W5-B 语料）：`__FILE__`/`__LINE__` 在生产核心 706 文件的日志调用中 **0 命中**（仅各测试 main 与 vendored cfitsio 使用）；`[[nodiscard]]` **0 命中**；`std::ignore` 0 命中；非 ACR 生产 `assert()` 0。
- 判别：该标准未强制"文件::符号"字面量（component 词表由 STRUCTURED_LOGGING 约定），故**不判为对标准的违反**，判为"标准要求的可定位三要素中 source 一环无任何机制承载"；同时"实际值 + 阈值"是否同现逐条不齐——正例 `cli/resource_gate.h:393-395`（实测值与 `0.85*min(workers,cpus)=阈值` 同现），反例 `lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp:484/502`（只报块 id，不给声明大小与实际解压值）。
- related：W5-N-07/08（码→名无表，同源）。

### W5-N-12 · P2 · 门面条 Python 吞错使被检分母静默缩水
- 计数：`tools/ ci/ scripts/` 162 文件中 `except` 298，体只有 `pass`/`continue` 的 64，裸 `except:` 3（清单见 §一）
- 危险子集（机器门自身）：`tools/quality/check_complexity.py:65` `except OSError: continue` —— 读不到的文件既不计入 `out["files"]` 也不进任何"跳过"桶，门在分母缩水后仍 PASS；`tools/quality/contracts/check_doc_symbols.py:51`、`check_full_integration.py:34`、`generate_contract_report.py:45` 三处裸 `except:` 位于合同检查器
- 非危险面（列出不报）：`tools/quality/check_source_inventory.py:38`、`check_comment_hygiene.py:36`、`tools/docs_machine_consistency.py:32` 的 `_deduce_root` 吞错后仍有两档确定回退
- 定级理由：宪章 §14.4 的禁令针对生产路径，故本条落点在"门禁可信度"（INDEX §三"机器门不红"总述的旁支），P2。
- related：M8-F-008、M8-F-010、簇「采集面静默缩水」。

### W5-N-13 · P2 · 损坏块解压的 8 轮 ×4 扩容重试（无字节封顶）
- 位置：`lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp:476`（ZSTD）、`:494`（LZ4），循环体 `estSize = blk->size*8; … estSize *= 4;`
- 静态事实：第 k 次尝试的目标缓冲 = `blk->size × 8 × 4^k`，k 最大 7 → 峰值 `131072 × 压缩块声明大小`；**除尝试次数外无字节上限**；彻底失败后 `result.clear()` + `fprintf`（`:484-486`、`:502-504`）返回空容器 → 调用面以"空块"继续
- 宪章对照：§14.4 禁"重复重试"；§17.6 把"无界内存增长"列为发布门禁禁止项。是否真分配到该量级取决于 `resize` 抛 `bad_alloc` 的时机 → 需运行期，未判；**上限倍数与无字节封顶是静态可判的**。
- related：V18-N-02（ahpx 读面缺字节封顶，机制级）。

### W5-N-14 · P2 · HISS 产品头 GAIN 解析吞错（元数据静默）
- 位置：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1149`（直写路径）、`:2275`（分块路径），两处同构 `try { hmeta.gain = std::stod(gain_str); } catch (...) {}`
- 事实：源帧 FITS 头带 `GAIN` 但值不可解析时，`hmeta.gain` 保持结构体声明处初值 → HISS 产品头写出"看起来是产品事实、实为默认"的增益；无日志、无计数、不影响 rc
- 与已报关系：`lib/astro_image_io/src/aio_fits.cpp:161/162`（BSCALE/BZERO）已由 **V10-N-04** 登记；本条只报未登记的**同机制新站点**，不重报该文件
- 下游是否进入科学链路：需运行期（HISS GAIN 消费者未在本轴核查），未判。

### W5-N-15 · P2 · `std::stod` 助手把"解析失败"折叠成"键不存在"
- 位置：`lib/astro_image_io/src/aio_fits.cpp:389` `try { return std::stod(v); } catch (...) { return def; }`（FITS 数值关键字通用取值助手）、`:491` `catch (...) { cal.ccd_temp = 0.0; }`
- 事实：助手把所有数值关键字的"解析失败"与"键不存在/空值"折叠为同一个默认值，调用方拿不到区分信号；`:491` 把 CCD-TEMP 解析失败静默变 0.0，而 0.0 是物理上合法的 CCD 温度 → 事后无法辨别"没有温度"与"温度解析失败"
- 与已报关系：同文件的 BSCALE/BZERO 站点属 V10-N-04；本条只报 `:389/:491` 两个未登记站点。
- related：V10-N-04、M2b-C-03、M5b-G-11。

---

## 三、错误码空间：复用 / 冲突 / 含义漂移（静态清点，W5-B + `include/`）

| 命名空间 | 定义位置 | 值域 | 与本轴相关的碰撞点 |
|---|---|---|---|
| CLI ExitCode（冻结 11 码） | `cli/exit_codes.h`（自称唯一源） | 0,2,3,4,5,6,7,8,9,10,70 | 10=RESOURCE；`jsonl_event_v1.schema.json` 的 `exit_code` enum 与之一致 |
| core::ExitCode（重复字面量表） | `include/astrocs/core/contracts.h:41-46` | 同上 | 注释自称不重定义、实际重列全部数值；无消费者；在"唯一源"门扫描范围外（W5-N-10） |
| acs_status（modular ABI） | `include/astrocs/abi/status_codes.h:153-166` | 0..10,70 | 8=BUDGET 9=SELFTEST **10=EXCEPTION** |
| acs_status（legacy ABI） | `include/astrocs/common_abi_v1.h` | 0..9,70 | 同枚举名、**无 10**；宿主 TU 只 include 这份 → 值 10 退化 UNKNOWN（W5-N-08） |
| aio_status | `include/astrocs/io/aio_abi_v1.h` | 0..15,70 | 8=TRUNCATED 9=BAD_HEADER **10=MISMATCH**（与 acs 的 8/9/10 含义完全不同） |
| aio_publish_status_v1 | `lib/hips/include/astrocs/hips/publish.h` | 0..15,70 | 与 aio_status 同谱，`_Static_assert` 齐备（`publish.h:130/134/139/143`，已核存在） |
| ACS_LOADER_EC_* | `runtime/module_loader/secure_loader.h` | 0..18,70 | 复用 `acs_error_info_v1.detail_code`（`status_codes.h:195` "模块自定义细分码; 0=无"），**无命名空间标记** |
| ACS_REG_EC_* | `runtime/registry/module_registry.h` | 0..14,70 | 同上，复用同一 uint32 字段（例：detail=5 在 loader 侧 = HASH_MISMATCH 族，在 registry 侧 = PATH_NOT_ABS 族；两处 `fill_err` 各自实现：`secure_loader.c:229`、`module_registry.c:376`） |
| AstroCsExitCode（orchestrator） | `lib/orchestrator/cpp/include/orchestrator.h` | 0..10、20..28、100+ | 与 CLI 11 码同键异义；其登记的权威表文件不存在（W5-N-09） |
| 测试/CI 侧 | `SKIP=77`（`ci/run.py` 注释引 ctest SKIP_RETURN_CODE）、124/127 | — | 在冻结码集外（W5-N-10） |

关于 `docs/architecture/ERROR_MODEL.md` 的"类别"词表（CONFIG / INPUT_CORRUPT / DEPENDENCY / NUMERIC / NO_DATA / RESOURCE / TIMEOUT / IO / SCIENCE_GATE / INTERNAL）与代码侧 `ACS_ERR_DOMAIN_*`（8 值）不同名不同数、且全仓无 `"category"` 字段发射点：**按准绳①②不判为缺陷**（文档分类学不是枚举合同；不得把与旧文档不一致当缺陷），只登记一条事实：`status_codes.h:168` 把域枚举的权威指向该文档，而该文档不含这 8 个名字 → 权威指向不闭合，若负责人认为应闭合须走 §1.2 变更流程，Agent 不自行改表。

---

## 四、§14.4 fail-fast 边界是否被降级成警告

宪章 §14.4（`ASTROCS_PROJECT_CONSTITUTION.md:548-556`）关键句：信任边界 = 「CLI/配置输入、外部文件解析、模块 C ABI、远程进程、持久化产品和用户可控数据」；禁止「无已知需求的 fallback、静默改默认值、宽泛 catch 后继续、重复重试、无限重试」；要求 fail-fast「发现合同违例立即返回明确状态码并停止当前产品提交」。

| 边界 | 判定 | 证据 |
|---|---|---|
| CLI/配置输入 | **未被降级**：`validate_config_full` 出口在 `cli/commands.cpp:1071/1204/1659` 直接退 INPUT | 边界本身成立 |
| 边界之后的重复解析 | **违例（静默改默认值）** | W5-N-05（6 处 `catch → "."`） |
| 外部文件解析（FITS/AHPX/HISS/JSON manifest） | **违例（宽泛 catch 后继续）** | W5-N-04/06/13/14/15 |
| 模块 C ABI 边界 | 设计有意且已处置（`status_codes.h:164` 规定异常转 ACS_ERR_EXCEPTION；`aio_api.cpp:414` void 边界屏障记录后吞）→ **判合规** | 缺陷只在宿主侧无该码名值表（W5-N-07/08） |
| 远程进程 | `run_process` 的 rc/timed_out/spawn_failed 在 `cli/commands.cpp:296-317` 显式区分并映射 → **判合规** | 仅映射值越界（W5-N-10） |
| 持久化产品 / 原子替换 | `cli/commands.cpp:455-458` rename 后查 `ec`；`lib/hips/src/aio_publish.cpp:370-374` 对 EXDEV/ENOTEMPTY 分类返回 → **判合规** | 父目录 fsync 丢弃见 W5-N-06 附带面 |
| 观测/诊断面 | **违例**：异常涂绿（W5-N-01）、cgroup/profile 回落不记录（W5-N-02/03） | — |

反例（正确处置，用于对照，不报）：`cli/commands.cpp:2397`（profile 不可解析 → `verdict="FAIL"`，无证据即失败）、`lib/core/src/module_adapters.cpp:4980-4986`（`Json::exception`/`bad_alloc`/`std::exception` 分别写入 manifest `error` 字段并 `Result::fail`）、`cli/commands.cpp:1240/1260`（CHK-002 读静态 IR 异常 → `mismatch=true`）。

**结论**：本轴未发现"把合同违例显式降级为 warning 事件"的新站点（`ev.emit(..., "warning", ...)` 集中在 `write_run_graphs` 渲染器缺失/超时，与 RT-009 冻结语义一致，属负责人意图）；真实形态是**降级为"连警告都不发"**，计入 W5-N-01/02/03/05。

---

## 五、重试与幂等

- 非 ACR 生产代码的 retry 语义只有 3 处：`lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp:476/494`（W5-N-13）与 `cli/process.cpp:177`（EINTR 单发重试，有界且必要）。
- ACR（`lib/acr/**`，164 文件）是重试集中区：`dispatcher.cpp:373/709/1638` 的 `kMaxAttempts`、attempt/重试队列、`focused_operations.cpp` 在 attempt>0 前重置 partial。**宪章 §10.1 明确 ACR 生产构建默认关闭、运行时不可达，Phase1/2/3 不得依赖 ACR 才能运行** → 本轴对 ACR 的重试次数、退避与幂等**不作静态判定**，一律记「需运行期 + 需接入裁决，未判」，避免把不可达路径写成 P0/P1 噪声。
- 幂等正证据（该面已被有意覆盖，不构成缺口）：`lib/cosmetic/tests/p1cos/p1cos_tests_core.cpp:201 check_i3_idempotent`；`lib/hips/src/aio_publish.cpp:355-374`（同名残留 staging 递归删除 + 目标非空拒收 → 发布可重放）；`lib/calibration/integration/astrocs_p1_calibration.integration.json:150/158`（重复驱动 = 调用方协议违规，由 RT-006 计数巡检，非静默）。
- 缺口：重试**没有字节/时间双封顶**，且失败后返回空容器而非错误码 → 重试与"吞错后走空返回"（SUMMARY 簇）在同一站点复合（W5-N-13）。

---

## 六、诊断可定位性（文件::符号 + 实际值 + 阈值）

| 要素 | 生产现状 | 证据 |
|---|---|---|
| 文件/行坐标 | 无机制承载（`__FILE__/`__LINE__` 生产 0 命中） | W5-N-11 |
| 符号名 | 靠消息里手写前缀（`[aio][ahpx][reader]`、`"DPSF"`、`"orchestrator"`），非统一、无编译期约束 | 抽样 `aio_ahpx_reader.cpp:484`、`lib/dynamic_psf/src/dpsf_psf.cpp:272-500`、`lib/orchestrator/cpp/src/orchestrator.cpp:128-181` |
| 实际值 + 阈值同现 | 门禁面最好、解析面最差 | 正例 `cli/resource_gate.h:393-395/433-434`；反例 `aio_ahpx_reader.cpp:484/502`、`aio_fits.cpp` 头解析消息不含路径与关键字名 |
| 码 → 名 | 无权威名值表（W5-N-07），宿主私有表漏值 10（W5-N-08） | — |
| 码 → troubleshooting | `docs/diagnostics/TROUBLESHOOTING.md` 10 场景 + `tools/astrocs_diagnose.py` 存在；登记事实源 CSV 缺失（W5-N-09），链条无机器校验 | — |
| 事件面形状 | `jsonl_event_v1.schema.json` 定 severity/type；`graph/warning/run_graphs` 事件用法正确 | `cli/commands.cpp:616-622` |

---

## 七、跨语言错误码名值表（结论）

**没有。** 现状是三份互不对齐的私有表 + 零个共享表：
1. C++：`lib/core/src/module_adapters.cpp:300 status_str`（11 值，缺 10）、`:317 to_result`（域映射，缺 10/7/9 → 默认 INTERNAL）
2. C（loader / registry 侧）：`runtime/module_loader/secure_loader.c:205 detail_message()`、`runtime/registry/module_registry.c:356 reg_msg()` —— 各自私有，名字不进任何共享表
3. Python：`tools/check_api_docs.py:27 EXIT_NAMES`（CLI 码名；其命名与 `exit_codes.h` 用词是否一致属 M5b-C-05 已报面，不重报）；**无任何** `ACS_ERR_*`/`AIO_ERR_*`/`detail_code` 名值映射
4. 冻结 ABI 已声明应有的跨语言表（`acs_status_name_v1`/`acs_status_domain_name_v1`）却无实现（W5-N-07）
5. 唯一有生产实现的名值表是 `acs_artifact_status_name_v1`（`runtime/artifact_store/artifact_abi_v1.c:624`）—— 说明"声明 + 实现"在本仓是既有约定，故 W5-N-07 是真缺口，不是风格差异。

---

## 八、条目总表（15 条；优先级按"数据破坏/伪成功 > 错误分类失真 > 可诊断性 > 卫生"）

| 编号 | 级 | 一句话 | 锚点（文件::行） | related（已报锚点，不重报） |
|---|---|---|---|---|
| W5-N-01 | P1 | 任务异常被吞后仍计执行数并写 status=COMPLETED，观测面伪成功 | lib/core/src/executor.cpp:137, :254（写盘点 :151） | M5a-G-005；簇「吞错后走空返回」 |
| W5-N-02 | P1 | 注释自称 handled 的 catch 使 profile 解析失败静默改 baseline | lib/backend_host/cpu_routing.cpp:412, :589 | M5a-G-010 |
| W5-N-03 | P1 | cgroup 限额解析失败按"无上限"处理（fail-open） | lib/backend_host/worker_advisor.cpp:72 | M5a-G-006、V7-N-08 |
| W5-N-04 | P1 | 退出码归因链吞 manifest 解析错 + 归因取"任一节点" | cli/runtime_client.cpp:389, :416 | M5b-C-05 |
| W5-N-05 | P1 | 配置文本解析失败 → 产物落点静默改 "."（6 处同构） | cli/commands.cpp:867, :1051, :1149, :1214, :1377, :1727 | V7-N-03、M5b-C-06 |
| W5-N-06 | P1 | file_sha256 的 ok 出参被丢弃/失败写空串 → 空串冒充哈希进 provenance | cli/commands.cpp:120, :138, :169, :466, :542, :553, :579, :1884（另有 :1339, :1342, :1351, :1718 写 `""`） | M8a-G-010、M6a-G-002、M2b-C-01 |
| W5-N-07 | P1 | 冻结 ABI 声明的跨语言名值表 0 定义 | include/astrocs/abi/status_codes.h:210, :211 | M5b-E-04 |
| W5-N-08 | P1 | 宿主私有表漏 ACS_ERR_EXCEPTION=10；7/9/10 域折叠 INTERNAL | lib/core/src/module_adapters.cpp:300-315, :317-329 | M5b-E-04、M5b-G-13、M5b-C-05 |
| W5-N-09 | P1 | 被引用的错误码权威登记表 CSV 不存在 | lib/orchestrator/cpp/include/orchestrator.h:111；tools/gen_audit_pack.py:74 | —（账本 0 命中） |
| W5-N-10 | P2 | 124/127 越界退出码；第二份字面量码表在唯一源门盲区 | cli/commands.cpp:306, :309；include/astrocs/core/contracts.h:41-46；tests/cli/test_cli_protocol.py::test_10_exit_codes_single_source | M5b-C-05、M5b-E-03、V18-N-05 |
| W5-N-11 | P2 | 生产诊断无坐标承载；状态返回值无 nodiscard 约束 | 全仓生产 706 文件（0 命中型） | W5-N-07/08 |
| W5-N-12 | P2 | 门面条 Python 吞错使被检分母静默缩水（64 处 pass/continue + 3 裸 except） | tools/quality/check_complexity.py:65 等 | M8-F-008、M8-F-010 |
| W5-N-13 | P2 | 解压 8 轮 ×4 扩容重试，峰值 131072×，无字节封顶 | lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp:476, :494 | V18-N-02 |
| W5-N-14 | P2 | HISS 产品头 GAIN 解析吞错，元数据静默取默认 | lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1149, :2275 | V10-N-04（同机制，已报站点不同） |
| W5-N-15 | P2 | stod 助手把"解析失败"折叠成"键不存在"（CCD-TEMP→0.0） | lib/astro_image_io/src/aio_fits.cpp:389, :491 | V10-N-04、M2b-C-03、M5b-G-11 |

结构：P1 ×9、P2 ×6；无 P0（本轴未找到"已确证的静默科学错误"——最接近的 W5-N-01/05/06 都需要运行期确认可达性，按"宁窄而实"不上抬）。

---

## 九、判「缺失」的三级复核（命令 + 输出，全部只读）

### 9.1 `acs_status_name_v1` / `acs_status_domain_name_v1` 无定义

L1 —— 受版本管理文件内按符号搜（排除 `run/` 影子树与本档案目录）：

```
$ git --no-optional-locks grep -n "acs_status_name_v1\|acs_status_domain_name_v1" -- ':!run' ':!问题扫描'
include/astrocs/abi/lifecycle_v1.h:236:/* 实例状态文本（诊断/日志用; 非法值返回 NULL, 与 acs_status_name_v1 同规） */
include/astrocs/abi/status_codes.h:210:const char* acs_status_name_v1(acs_status s);
include/astrocs/abi/status_codes.h:211:const char* acs_status_domain_name_v1(int32_t domain);
[exit 0]
```

L2 —— 文件系统级按扩展名搜（不依赖 git，覆盖未跟踪文件；目录白名单为代码目录）：

```
$ grep -rn "acs_status_name_v1" --include=*.c --include=*.cpp --include=*.h --include=*.py --include=*.txt --include=*.cmake \
      lib cli include runtime providers modules tests tools ci scripts cmake
include/astrocs/abi/status_codes.h:210:const char* acs_status_name_v1(acs_status s);
include/astrocs/abi/lifecycle_v1.h:236:/* 实例状态文本（诊断/日志用; 非法值返回 NULL, 与 acs_status_name_v1 同规） */
[exit 0]
```

L3 —— 按"定义形态"搜（排除以 `;` 结尾的声明行）：结果只剩注释行与档案脚本自身，无任何 `{ ` 形态定义体；`[exit 0]` 来自管道中的 grep 命中注释行。

同族对照（证明"声明 + 实现"是本仓既有约定）：

```
$ git --no-optional-locks grep -n "acs_lc_state_name_v1\|acs_artifact_status_name_v1\|acs_module_op_name_v1" -- ':!run'
include/astrocs/abi/lifecycle_v1.h:237:const char* acs_lc_state_name_v1(int state);
include/astrocs/abi/lifecycle_v1.h:238:const char* acs_module_op_name_v1(int op);
include/astrocs/contracts/artifact_abi_v1.h:85:const char* acs_artifact_status_name_v1(acs_artifact_status s);
runtime/artifact_store/artifact_abi_v1.c:624:const char* acs_artifact_status_name_v1(acs_artifact_status s) {
tests/abi/abi002_lifecycle_probe.c:101:const char* acs_lc_state_name_v1(int state)
tests/abi/abi002_lifecycle_probe.c:117:const char* acs_module_op_name_v1(int op)
[exit 0]
```

结论：status 族的两个名值表函数**只有声明**；lifecycle 族的两个**只有测试探针实现**；artifact 族**有生产实现**。

### 9.2 `engineering/contracts/error_code_registry.csv` 不存在

```
$ git --no-optional-locks ls-files | grep -n "error_code_registry" ; echo "rc=$?"
rc=1
$ ls engineering/contracts/ | grep -i "error\|code"
（无输出）
```

引用方（两处，均把该 CSV 当事实源）：

```
$ git --no-optional-locks grep -n "error_code_registry"
lib/orchestrator/cpp/include/orchestrator.h:111:  ... 权威见 engineering/contracts/error_code_registry.csv
tools/gen_audit_pack.py:74:  ... "engineering/contracts/error_code_registry.csv"
[exit 0]
```

副本存在位置（非权威、非当前路径）：`设计大纲/_evidence/packs/history/…` 下的历史审核包副本；`git ls-files` 中 `engineering/` 下无该文件。

### 9.3 免重报核对（逐关键词查账本，脚本 `scripts_w5_dedup*.py`）

```
$ python3 -B 问题扫描/_verify/scripts_w5_dedup2.py   # 26 个关键词 × 682 行账本
tasks_executed => 0        executor.cpp:13 => 0      worker_task => 0
observed_trace => 0        gen_run_graphs => 0       COMPLETED => 0
acs_status_name => 0       status_str => 0           detail_code => 0
LOGGING_DIAGNOSTICS => 0   error_domain_name => 0    error_code_registry => 0
bare except => 0           value_or => 0             filesystem::remove => 0
file_sha256 => 1 [M8a-G-010:P2]      module_adapters => 47 [M1a-C-003 … ]
exit_codes => 5 [M5b-C-03, M5b-C-05, M5b-E-03, M5b-G-15, M6b-E-004]
status_codes => 2 [M5b-E-04:P1, M5b-G-13:P1]        ACS_ERR_BUDGET => 1 [M6a-D-002:P1]
aio_fits.cpp:16 => 0（故 V10-N-04 只覆盖 BSCALE/BZERO 语义，未覆盖 :389/:491 站点）
stage2.cpp:17 => 0          orchestrator.cpp:3094 => 0        photometry_report => 0
```

已报同文件簇（本次全部改挂 related 而非新报）：executor.cpp → M5a-G-005；worker_advisor.cpp → M5a-G-006 / V7-N-08；cpu_routing.cpp → M5a-G-010；ipv_entry.cpp → V2-N-01 / M1a-E-001 / M6a-D-009；aio_fits.cpp → M2b-C-03 / M5b-G-11 / V10-N-04；publish.h → M9-C-1；aio_abi_v1.h → M5b-G-13；secure_loader/module_registry → M5b-G-06 / M8-F-001 / M6a-E-001 / M6a-I-003；create_directories → M6a-G-002；fsync → M2b-C-01（等 5 条）。

INDEX §三 相关唯一总述（本轴只挂支线，不抢总述）：机器门不红（M8-G-001 + M6b-G-001 + L28b）、边界校验做一半（M9.md §6）、修复即扩散/反向钉死（M9 §7 六站点 + M8 §0.3 + M5b 版本簇）。

---

## 十、负结果（查过、判为无缺陷或未找到，防止后人重做）

1. **`[[nodiscard]]` / `std::ignore` 全仓生产 0 命中**——但宪章 §14.4 未要求标注，故只作为 W5-N-11 的事实面，不单独成条。
2. **`lib/hips/include/astrocs/hips/publish.h` 声称的 `_Static_assert` 对齐确实存在**（`:130/134/139/143`）——"声明与实现对齐"在此处成立，无缺陷。
3. **`cli/commands.cpp:443/483/486 create_directories(..., ec)` 的 ec 未查不构成缺陷**：紧邻的 `ofstream` 打开失败会返回 `astrocs::IO`（`:448-451`、`:492-495`），错误仍被上层捕获，只损失"目录错 vs 文件错"的区分度——记为 W5-N-06 的附带面，不单独成条。
4. **`lib/hiss_stream_writer.cpp:119/122/642/685/688 filesystem::remove(path, ec)`**：`cleanup_temp_files` 的语义是"尽力清理半成品"，其注释（`:110-114`）自证有意不回滚正式文件；失败不记录属可诊断性弱点，不构成产品错误。
5. **`cli/process.cpp:102/125/126/129`、`cli/monitor.h:124` 的 `CloseHandle`**：清理路径上的句柄关闭，忽略返回值不影响状态（Windows 语义下 CloseHandle 失败无产品后果）。
6. **`providers/cpu/*/run_banded` 的 `(void)run_banded`**：是"未用参数抑制"，不是丢弃返回值。
7. **`lib/phase2/tools/stage2.cpp:1775/1779`、`lib/orchestrator/cpp/src/main.cpp:157`、`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp:334/347/360`、`lib/astro_image_io/src/aio_api.cpp:414`** 判为合理吞错（判据与理由见 §一末）。
8. **JSONL schema 与 CLI 码集一致**：`contracts/schemas/jsonl_event_v1.schema.json` 的 `exit_code` enum 与 `cli/exit_codes.h` 完全一致，未发现 schema 漂移。
9. **`docs/architecture/ERROR_MODEL.md` 类别表与代码域枚举不同**——按准绳①②不判缺陷（见 §三末段说明），只登记"权威指向不闭合"事实。
10. **ACR 重试面未判**：`kMaxAttempts`、attempt 重置、重试队列在 164 个文件里成体系，但 §10.1 使其运行时不可达，静态不判（§五）。
11. **未在本轴找到"生产路径把合同违例 emit 成 warning 事件"的新站点**（§四结论）；已核 `ev.emit(..., "warning", ...)` 全部出现点均落在 RT-009 允许的不失败面。
12. **`lib/astro_image_io/src/aio_fits.cpp:156` BITPIX 解析**：`bytes_per_pixel==0` 时有 `return -1` 前置校验（`:163-165` 区段），未构成"静默按 8bit 处理"，故不列条目（修正早期筛查假设）。

---

## 十一、排除清单（W5-A → W5-B 的 144 个文件，全量，按 `./x`）

分组计数：cfitsio 105、healpix_stack(archive/legacy) 29、json-schema-validator 9、nanoflann 1。

```
./lib/astro_image_io/third_party/cfitsio/buffers.c
./lib/astro_image_io/third_party/cfitsio/cfileio.c
./lib/astro_image_io/third_party/cfitsio/cfortran.h
./lib/astro_image_io/third_party/cfitsio/checksum.c
./lib/astro_image_io/third_party/cfitsio/drvrfile.c
./lib/astro_image_io/third_party/cfitsio/drvrgsiftp.c
./lib/astro_image_io/third_party/cfitsio/drvrgsiftp.h
./lib/astro_image_io/third_party/cfitsio/drvrmem.c
./lib/astro_image_io/third_party/cfitsio/drvrnet.c
./lib/astro_image_io/third_party/cfitsio/drvrsmem.c
./lib/astro_image_io/third_party/cfitsio/drvrsmem.h
./lib/astro_image_io/third_party/cfitsio/editcol.c
./lib/astro_image_io/third_party/cfitsio/edithdu.c
./lib/astro_image_io/third_party/cfitsio/eval_defs.h
./lib/astro_image_io/third_party/cfitsio/eval_f.c
./lib/astro_image_io/third_party/cfitsio/eval_l.c
./lib/astro_image_io/third_party/cfitsio/eval_tab.h
./lib/astro_image_io/third_party/cfitsio/eval_y.c
./lib/astro_image_io/third_party/cfitsio/f77_wrap.h
./lib/astro_image_io/third_party/cfitsio/f77_wrap1.c
./lib/astro_image_io/third_party/cfitsio/f77_wrap2.c
./lib/astro_image_io/third_party/cfitsio/f77_wrap3.c
./lib/astro_image_io/third_party/cfitsio/f77_wrap4.c
./lib/astro_image_io/third_party/cfitsio/fits_hcompress.c
./lib/astro_image_io/third_party/cfitsio/fits_hdecompress.c
./lib/astro_image_io/third_party/cfitsio/fitscore.c
./lib/astro_image_io/third_party/cfitsio/fitsio.h
./lib/astro_image_io/third_party/cfitsio/fitsio2.h
./lib/astro_image_io/third_party/cfitsio/getcol.c
./lib/astro_image_io/third_party/cfitsio/getcolb.c
./lib/astro_image_io/third_party/cfitsio/getcold.c
./lib/astro_image_io/third_party/cfitsio/getcole.c
./lib/astro_image_io/third_party/cfitsio/getcoli.c
./lib/astro_image_io/third_party/cfitsio/getcolj.c
./lib/astro_image_io/third_party/cfitsio/getcolk.c
./lib/astro_image_io/third_party/cfitsio/getcoll.c
./lib/astro_image_io/third_party/cfitsio/getcols.c
./lib/astro_image_io/third_party/cfitsio/getcolsb.c
./lib/astro_image_io/third_party/cfitsio/getcolui.c
./lib/astro_image_io/third_party/cfitsio/getcoluj.c
./lib/astro_image_io/third_party/cfitsio/getcoluk.c
./lib/astro_image_io/third_party/cfitsio/getkey.c
./lib/astro_image_io/third_party/cfitsio/group.c
./lib/astro_image_io/third_party/cfitsio/group.h
./lib/astro_image_io/third_party/cfitsio/grparser.c
./lib/astro_image_io/third_party/cfitsio/grparser.h
./lib/astro_image_io/third_party/cfitsio/histo.c
./lib/astro_image_io/third_party/cfitsio/imcompress.c
./lib/astro_image_io/third_party/cfitsio/iraffits.c
./lib/astro_image_io/third_party/cfitsio/iter_a.c
./lib/astro_image_io/third_party/cfitsio/iter_b.c
./lib/astro_image_io/third_party/cfitsio/iter_c.c
./lib/astro_image_io/third_party/cfitsio/longnam.h
./lib/astro_image_io/third_party/cfitsio/modkey.c
./lib/astro_image_io/third_party/cfitsio/pliocomp.c
./lib/astro_image_io/third_party/cfitsio/putcol.c
./lib/astro_image_io/third_party/cfitsio/putcolb.c
./lib/astro_image_io/third_party/cfitsio/putcold.c
./lib/astro_image_io/third_party/cfitsio/putcole.c
./lib/astro_image_io/third_party/cfitsio/putcoli.c
./lib/astro_image_io/third_party/cfitsio/putcolj.c
./lib/astro_image_io/third_party/cfitsio/putcolk.c
./lib/astro_image_io/third_party/cfitsio/putcoll.c
./lib/astro_image_io/third_party/cfitsio/putcols.c
./lib/astro_image_io/third_party/cfitsio/putcolsb.c
./lib/astro_image_io/third_party/cfitsio/putcolu.c
./lib/astro_image_io/third_party/cfitsio/putcolui.c
./lib/astro_image_io/third_party/cfitsio/putcoluj.c
./lib/astro_image_io/third_party/cfitsio/putcoluk.c
./lib/astro_image_io/third_party/cfitsio/putkey.c
./lib/astro_image_io/third_party/cfitsio/quantize.c
./lib/astro_image_io/third_party/cfitsio/region.c
./lib/astro_image_io/third_party/cfitsio/region.h
./lib/astro_image_io/third_party/cfitsio/ricecomp.c
./lib/astro_image_io/third_party/cfitsio/scalnull.c
./lib/astro_image_io/third_party/cfitsio/simplerng.c
./lib/astro_image_io/third_party/cfitsio/simplerng.h
./lib/astro_image_io/third_party/cfitsio/swapproc.c
./lib/astro_image_io/third_party/cfitsio/utilities/cookbook.c
./lib/astro_image_io/third_party/cfitsio/utilities/fitscopy.c
./lib/astro_image_io/third_party/cfitsio/utilities/fitsverify.c
./lib/astro_image_io/third_party/cfitsio/utilities/fpack.c
./lib/astro_image_io/third_party/cfitsio/utilities/fpack.h
./lib/astro_image_io/third_party/cfitsio/utilities/fpackutil.c
./lib/astro_image_io/third_party/cfitsio/utilities/ftverify.c
./lib/astro_image_io/third_party/cfitsio/utilities/funpack.c
./lib/astro_image_io/third_party/cfitsio/utilities/fverify.h
./lib/astro_image_io/third_party/cfitsio/utilities/fvrf_data.c
./lib/astro_image_io/third_party/cfitsio/utilities/fvrf_file.c
./lib/astro_image_io/third_party/cfitsio/utilities/fvrf_head.c
./lib/astro_image_io/third_party/cfitsio/utilities/fvrf_key.c
./lib/astro_image_io/third_party/cfitsio/utilities/fvrf_misc.c
./lib/astro_image_io/third_party/cfitsio/utilities/imcopy.c
./lib/astro_image_io/third_party/cfitsio/utilities/iter_image.c
./lib/astro_image_io/third_party/cfitsio/utilities/iter_var.c
./lib/astro_image_io/third_party/cfitsio/utilities/smem.c
./lib/astro_image_io/third_party/cfitsio/utilities/speed.c
./lib/astro_image_io/third_party/cfitsio/utilities/testprog.c
./lib/astro_image_io/third_party/cfitsio/vmsieee.c
./lib/astro_image_io/third_party/cfitsio/wcssub.c
./lib/astro_image_io/third_party/cfitsio/wcsutil.c
./lib/astro_image_io/third_party/cfitsio/win_compat/unistd.h
./lib/astro_image_io/third_party/cfitsio/windumpexts.c
./lib/astro_image_io/third_party/cfitsio/zcompress.c
./lib/astro_image_io/third_party/cfitsio/zuncompress.c
./lib/healpix_db/archive/legacy/healpix_stack/ahps_format.h
./lib/healpix_db/archive/legacy/healpix_stack/ahps_reader.cpp
./lib/healpix_db/archive/legacy/healpix_stack/ahps_reader.h
./lib/healpix_db/archive/legacy/healpix_stack/ahps_writer.cpp
./lib/healpix_db/archive/legacy/healpix_stack/ahps_writer.h
./lib/healpix_db/archive/legacy/healpix_stack/gradient/corrected_stacker.cpp
./lib/healpix_db/archive/legacy/healpix_stack/gradient/corrected_stacker.h
./lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_fitter.cpp
./lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_fitter.h
./lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_sampler.cpp
./lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_sampler.h
./lib/healpix_db/archive/legacy/healpix_stack/gradient/nanoflann.hpp
./lib/healpix_db/archive/legacy/healpix_stack/gradient/snr_evaluator.cpp
./lib/healpix_db/archive/legacy/healpix_stack/gradient/snr_evaluator.h
./lib/healpix_db/archive/legacy/healpix_stack/gradient/spherical_spline.cpp
./lib/healpix_db/archive/legacy/healpix_stack/gradient/spherical_spline.h
./lib/healpix_db/archive/legacy/healpix_stack/gradient/test_gradient_sampler.cpp
./lib/healpix_db/archive/legacy/healpix_stack/gradient/test_snr_evaluator.cpp
./lib/healpix_db/archive/legacy/healpix_stack/healpix_core.cpp
./lib/healpix_db/archive/legacy/healpix_stack/healpix_core.h
./lib/healpix_db/archive/legacy/healpix_stack/hp_stack_api.cpp
./lib/healpix_db/archive/legacy/healpix_stack/hp_stack_api.h
./lib/healpix_db/archive/legacy/healpix_stack/hp_stack_hiss.cpp
./lib/healpix_db/archive/legacy/healpix_stack/hp_stack_hiss.h
./lib/healpix_db/archive/legacy/healpix_stack/simple_test.cpp
./lib/healpix_db/archive/legacy/healpix_stack/stack_db.cpp
./lib/healpix_db/archive/legacy/healpix_stack/stack_db.h
./lib/healpix_db/archive/legacy/healpix_stack/stack_engine.cpp
./lib/healpix_db/archive/legacy/healpix_stack/stack_engine.h
./lib/healpix_db/healpix_drizzle/nanoflann.hpp
./lib/orchestrator/cpp/third_party/json-schema-validator/json-patch.cpp
./lib/orchestrator/cpp/third_party/json-schema-validator/json-patch.hpp
./lib/orchestrator/cpp/third_party/json-schema-validator/json-schema-draft7.json.cpp
./lib/orchestrator/cpp/third_party/json-schema-validator/json-uri.cpp
./lib/orchestrator/cpp/third_party/json-schema-validator/json-validator.cpp
./lib/orchestrator/cpp/third_party/json-schema-validator/nlohmann/json-schema.hpp
./lib/orchestrator/cpp/third_party/json-schema-validator/smtp-address-validator.cpp
./lib/orchestrator/cpp/third_party/json-schema-validator/smtp-address-validator.hpp
./lib/orchestrator/cpp/third_party/json-schema-validator/string-format-check.cpp

```

排除口径复算（一行）：

```
python3 -B - << 'PY'
import subprocess
R="/workspace/Astro CS Database"
f=subprocess.run(["git","--no-optional-locks","ls-files"],cwd=R,capture_output=True,text=True).stdout.splitlines()
p=[x for x in f if x.endswith((".cpp",".h",".hpp",".c")) and x.startswith(("lib/","cli/","include/","runtime/","providers/","modules/"))]
print(len(p), len([x for x in p if not any(v in x for v in ("/archive/","/legacy/","nanoflann","/browser_qt/","third_party"))]))
PY
```

预期输出：`850 706`（W5-A → W5-B）。

---

## 十二、本轴遗留（交下一轮或负责人裁决）

1. W5-N-01 / N-05 / N-06 / N-13 / N-14 的**可达性**都需运行期证据（任务体是否自带 catch；cfg_text 在边界后是否可能不可解析；清单写盘时文件是否可能不可读；损坏 AHPX 块；源帧 GAIN 非数值）。本轴按"宁窄而实"全部标「需运行期，未判」，不上抬优先级。
2. `ACS_ERR_EXCEPTION` 的产生面（模块化 ABI 模块）与宿主消费面（legacy 表）之间是否存在生产调用链，属"模块装配可达性"，应由装配轴（M5b/M6a 族）判；本轴只登记名值表缺项。
3. `detail_code` 的跨模块命名空间分配（loader 0-18 / registry 0-14 / AIO 0-15 复用同一 uint32 字段）需负责人裁决是否新建登记表并纳入机器门；本轴不擅自提议新表结构。
4. `write_run_graphs`（`cli/commands.cpp:479-629`）在 7 个失败出口静默 `return`（`:487/:493/:495/:570/:572/:589/:591`）且不发 warning 事件，与该文件 `:593-598` 自述的"产物缺失必须可诊断"不一致；因图产物按 RT-009 冻结语义"不失败 run"属负责人意图，本轴**未单列条目**，交由观测轴（M6b/L28 族）判定是否补 warning 事件。
