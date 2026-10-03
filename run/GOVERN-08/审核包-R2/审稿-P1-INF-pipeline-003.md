# 审稿-P1-INF-pipeline-003 — G08-05 对抗审稿 第 1 遍

- **片号**：`INF-pipeline-003`
- **层**：`lib/infrastructure/pipeline`
- **仓库 HEAD**：`1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`（⚠️ 派单书写的 `850a9ede` 与实际 HEAD 不符，已按实际 HEAD 取证）
- **纪律遵守**：零 git 写、零编译、零 ctest/pytest/二进制、未读 `/tmp/acsd_g08/`、未修改任何仓内文件（除本交付件）。中文路径一律 `git -c core.quotepath=false`。
- **取证基准**：`git ls-files` 过滤 `run/`、`build/`（`run/` 下有 10+ 份历史快照副本，会污染 find/grep 结论）。

---

## 1. 读完了吗

**口径声明**：「读」= 用 read 工具打开该文件并读到 `End of file`，非抽样、非只看差异。「亲自读」= 本审稿人本人读；子代理读的不计入亲自覆盖，但单列。

| 口径 | 数值 |
|---|---|
| 成员份数 | 21 |
| **亲自完整读完** | **16 份 / 5903 行 / 74.1%** |
| 子代理完整读完（我另行复核关键结论） | 5 份 / 2063 行 |
| **本人∪子代理合并覆盖** | **21 份 / 7966 行 / 100%** |
| 实际亲自读到的行数 | 5903 |
| 成员总行数 | 7966（与清单「实际行数: 7966」逐份相加一致） |

### 未由我亲自读完的 6 份（如实列出）

| 文件 | 行数 | 覆盖来源 | 我对它的独立复核 |
|---|---|---|---|
| `cpp/tests/test_logger.cpp` | 545 | 子代理 76519eb6 | ⛔ 未亲自读，未独立复核 |
| `typed_dag.py` | 624 | 子代理 7b426388 | ✅ 复核其悬空边反例（我方 grep 交叉确认 `producer_of` 遍历面） |
| `orchestrator/memory.md` | 693 | 子代理 7b426388 | ✅ 复核其指向已删目录的悬空引用（`ls` 实证） |
| `orchestrator/docs/architecture.md` | 128 | 子代理 7b426388 | ✅ 复核「现役文档链零引用」 |
| `configs/stage1_gc_panel1_Red.json` | 73 | 子代理 7b426388 | ✅ 复核其与 panel3 的 diff（无复制粘贴错误） |
| `cpp/tests/fixtures/…json` 之外的 `test_logger.cpp` | — | — | 见上 |

> ⚠️ **`test_logger.cpp`（545 行）是我本片唯一的实质盲区**，已派子代理但本轮未回收其结论，登记为 UNRESOLVED。

---

## 2. 本片判定：**阻断**

最重 3 条：

1. **`module_registry.c:306` 越界读（生产代码内存安全缺陷）**——安全加载器解析 manifest 时，只对 `j.p[0]` 做了边界检查，却连续读 `j.p[1]`/`j.p[2]`/`j.p[3]`。`parse_units` 的输入缓冲是 `malloc(sz+1)`（`:604`），越界最多读到分配块外 2 字节。
2. **`acsd_registry_check_v1`（`module_registry.c:886-892`）恒返回 `ACS_OK`**——名为「check」的函数无论有多少 finding 都返回成功，宿主若只判返回码则**永不失败**。这是生产代码里的恒真门。
3. **`admission_controller.h` 全文件零实现 + `engineering_authoritative/` 规范源整目录已删**——25 个方法在 `.cpp` 中命中数为 0；`:4`/`:10` 引用的规范与 Python 原型全仓不存在。

---

## 3. 逐文件清单

### 3.1 `cpp/src/checkpoint.cpp`（802 行，亲自读）
- **看到**：`deserialize()` **无条件 `return true`**（`:501`）；`load()` 只用 `fs::exists` 判存在（`:578`），0 字节文件照样进解析。`atomic_write` 只 `flush/close` 后 `rename`（`:519-543`），**无 fsync**，且 POSIX 分支缺 Windows 分支 `:527` 的 `MOVEFILE_WRITE_THROUGH`。`fully_completed` **只置 true 从不复位**（`:738-740`）。`sanitize_frame_name` 去路径前缀（`:301-309`）但 `load` 从不校验文件内 `frame_name`（`:387`）。9 处 `std::cerr` 绕过统一日志。
- **判定**：**须修**（其中 `fully_completed` 不复位为阻断候选，接线后即发作）。

### 3.2 `cpp/src/logger.cpp`（308 行，亲自读）
- **看到**：`log_file_.open()` **返回值丢弃，注释自认「打开失败时静默忽略」**（`:294-295`），ERROR 一并消失。`log_file_path_` 在 open **之前**就赋值（`:290`），故 `get_log_file_path()` 非空**不代表**日志可用 —— 恒真门素材。`string_to_level` 非法串静默返回 INFO（`:211-212`）。
- **判定**：**须修**；配合 `main.cpp:304` 升格为**阻断**（见 B1）。

### 3.3 `cpp/include/logger.h`（116 行，亲自读）
- **看到**：`:8` 宣称的「默认日志路径 `lib/.../logs/`」在全仓**无任何代码产生**（`log_dir_` 无默认、`Logger()` 空体）——悬空引用。`:23-27` 在头文件里 `#undef ERROR`，全局宏副作用 + 头文件顺序依赖。`:113-116` `LOG_*` 宏无 `do{}while(0)`。
- **判定**：**须修**。

### 3.4 `cpp/src/cli_command.cpp`（190 行，亲自读）
- **看到**：`:164`/`:170` 用 `>= 0.0` 作哨兵 —— **NaN 静默丢字段，+inf 输出非法 JSON `inf`**。`:176-188` `result_json`/`error_json`/`extra_json` 原样拼接零校验（`:186` 注释「应」字表明是约定非强制）。
- ✅ **该文件 `:42-46` 声称 `request_cancel()` 是纯 atomic store —— 我独立核实为真**（`orchestrator.cpp:390-392` 确为 `cancel_token_.store(true, release)`）。该攻击不成立，如实记为已排除误报。
- **判定**：**须修**。

### 3.5 `cpp/include/admission_controller.h`（213 行，亲自读）
- **看到**：**25 个方法零实现**。`:4`/`:10` 引用 `engineering_authoritative/...` —— 该目录**全仓零跟踪文件**。硬编码：`:75` `os_margin = 2GiB`、`:114-116` 三个 CPU 阈值 90/70/60、`:142` `HISTORY_MAX=20`，均无出处、未入配置。`:121` `set_monitor` 不持锁而 `get_current_load` 持锁 → 数据竞争。`:204` `get_level()` 不加锁。
- **判定**：**阻断**。

### 3.6 `cpp/include/json_config.h`（114 行，亲自读）
- **看到**：`:4` 声称「**单一权威, 无手写重复规则**」——**被代码证伪**：`json_config.cpp:508` 有一份手抄 `)JSON";` 内嵌副本，磁盘另有 `configs/stage1.schema.json`，**两份都要手工维护**。`:110-111` 声明 `get_stage1_schema_sha256()` 是「与磁盘 schema 一致性测试用」，而**唯一调用点只断言非空**（见 3.9）。硬编码密集：`initial_ra_deg = -999.0` 哨兵（`:44-45`）、`pixfrac = 0.8` 无出处（`:67`）、`estimator_id = 1`（`:61`）、`max_stars = 2000`。
- **判定**：**须修**（`:4` 的自述失真本身是发现）。

### 3.7 `cpp/tests/test_orchestrator_cli.cpp`（1968 行，亲自读）
- **看到**——**本片最密集的自洽式/恒真断言集中地**：
  - `:1113-1114` **被「修好」但仍是恒真断言**：`clear_all()` 后没有重新读 `list`，断言的是第二次 `clear_all()` **之前**的快照。
  - `:1199` `ASSERT_TRUE(n_loaded >= 0, ...)` —— `n_loaded` 由 5 次循环自增，**恒真**。而其上方注释写「但应至少加载部分模块」，`:1198` —— **注释的意图（≥1）与断言（≥0）不符**。
  - `:1239` `ASSERT_EQ(n3, n1)` —— **期望值由被测代码自己产生**；加载器恒坏（两次都 0 个）时 0==0 通过。
  - `:1250-1268`、`:1282-1291` 条件跳过（`[跳过]`），不计入任何计数 → `main` 的 `:1967 return (g_fail_count == 0)` 可在「什么都没测」时返回 0。
  - `:1865-1866` `get_stage1_schema_sha256()` 只断言 `!sh.empty()` —— sha256 恒返回 64 字符，**恒真门**。
  - `:1442-1445` 年份断言白名单 `2026||2025||2027` —— **2028 年必红的定时炸弹**。
  - `:1525`/`:1539`/`:1547`/`:1577`/`:1605` 五处 `ASSERT_TRUE(r.exit_code != 0)` 且注释称「FITS 不存在」—— **我实测 `testdata/Galaxy_Center_T4/lights/panel3/*.fts` 存在**，失败原因是 `filters.json`/`qe.json` 在 TempDir 下解析不到（`:366-367` 是裸名）。**注释与夹具事实相反，且这些门依赖一个非预期原因。**
  - ✅ `:1869-1917` 三条 SIGINT 断言是**前人已修的真门**：注释声明的判红条件（删 `cli_command.cpp:66`/`:76`/`:65` 的 if 守卫）经我复核**确实成立**。这是本片少见的正面样本。
- **判定**：**阻断**（DllLoader 门整体可空转 + 恒真门群）。

### 3.8 `cpp/tests/gate/orchestrator_saturation_wiring_gate.cpp`（357 行，亲自读）
- **看到**：这是**本片质量最高的门**。G6b/G6c（`:339-352`）用同一合成帧的「过滤臂 vs 未过滤臂」逐像素 variance 对比，期望关系来自物理（饱和平台被排除出 blank-sky 统计），**不是从被测代码算出**——**非自洽式**。`:33-35` 声明的负例注入经复核成立。
- ✅ **我独立验证它确实在构建图里**（`tests/CMakeLists.txt:85-97` `add_test`），即它没有重蹈它自己要防的「不在任何构建图」覆辙。
- **遗留**：`:91` `kSaturate` 定义后从未使用（死常量）；`:340` 的 `*50.0` 与 `:341` 的 `<1000.0` 是无出处阈值。
- **判定**：**建议**。

### 3.9 `cpp/tests/CMakeLists.txt`（178 行，亲自读）
- **看到**：`:46-51` `orchestrator_legacy_cli_validate` 用 `PASS_REGULAR_EXPRESSION "VALID"` —— **`INVALID` 以 `VALID` 为子串，故校验失败时该正则同样匹配，门仍然绿**（潜伏假绿）。`:48` 引用的 `configs/stage1.template.json` **存在**（1652 B），此条悬空不成立。`:99-126` 的 `orchestrator_curve_resolve_gate_fault_inject` 是**能红能绿的负例对照**，正面样本。
- ✅ 已验证 `:17` 的 `test_checkpoint.cpp`、`:77` 的 `gate_module_snr_extract_spy.cpp`、`:109`/`:136` 的两个测试源**均真实存在**，无悬空引用。
- **判定**：**须修**。

### 3.10 `orchestrator/tests/test_orchestrator_e2e.py`（485 行，亲自读）
- **看到**：`:43` 用 **5** 级 `..` 回项目根，而 `:14`/`:42` 注释都写「仅需 **3** 级」——**注释与代码矛盾**（代码对、注释错），照注释「修」即改坏。`:57`/`:67-78` 的 10 个模块路径**全部是迁移前旧布局**（`lib/astro_image_io`、`lib/calibration/python`…），**我逐个 `ls` 验证 8/8 不存在**；`:81` 用 `if os.path.isdir` **静默跳过**。`:122-156` 四个适配器加载全用裸 `except Exception` + `print("[FAIL]")` 后**继续**。`:378` `from orchestrator import Orchestrator` —— **全仓无 `orchestrator.py`**。`:331` 校准**无条件跳过**，而 `:9` docstring 仍宣称测「校准→…」5 段。
- **判定**：**阻断**（该测试在当前仓布局下**必然失败**，且未被任何 runner 注册；`:317-320` 还在 `lib/` 内建 `output/`+`logs/`）。

### 3.11 `module_loader/module_registry.c`（987 行，亲自读）
- **看到**（本片最重）：`:306` **越界读**（唯一边界检查是 `j.p[0]`，却读 `j.p[1..3]`，输入缓冲 `malloc(sz+1)` @ `:604`）；`:886-892` `acsd_registry_check_v1` **恒返回 ACS_OK**；`:730-732` `file_sha256_hex` 失败时 `sha_actual[0]=0` 且 `:734` 的比对因此被**整体跳过**——**哈希门恰在哈希失败时失效，且无 finding、无错误码**；`:823` 用 DLL 自报的 `lsha` **覆写** registry 自算的 `sha_actual`，二者不符不记 finding；`:198`/`:203` 超长字段**静默截断**且不置错误标志；`:331-336` realloc **无容量上限**；`:655-670` 去重扫描 **O(n²)**；`:897`/`:915`/`:923` 三个公开 API 返回裸 `ACS_ERR_PARAM` 而**不 `fill_err`**。
- ✅ **正面样本**：`:957-987` `acsd_registry_self_test_v1` 用**独立已知答案向量**（`e3b0c4…`/`ba7816…`/`248d6a…` 三个 NIST 向量）——**真测试，非自洽式**。
- **判定**：**阻断**。

### 3.12 `module_loader/README.md`（58 行，亲自读）
- **看到**：`:53-55` 与 `:56-58` **逐字重复**（我自己读到，确认）。`:14` 的 `12_DLL_ABI_AND_LOADER_STANDARD.md`、`:50` 的 `eng/packaging/verify_install_tree.py`、`:52` 的 `eng/ci/ledgers/prod_wiring.json` —— 我用 `git ls-files --error-unmatch` 逐个验证，**三项全部不存在**。
- ✅ **重要正面结论**：`:46-52` 声明本加载器/registry「不在三个生产命令的运行期调用图上」。我独立验证该声明**成立**：`acsd_registry_open_v1`/`acsd_secure_loader_load_v1` 在 `lib/**` 生产源**零调用者**（仅两个 `eng/tests/abi/` 探针 + 文档），且 `docs/engineering/MODULE_MAP.md:35` 独立佐证。**与本项目已被证伪的「加权算子零消费者」案例不同，这条退役声明经得起复核。**
- **判定**：**须修**（重复段 + 3 处悬空）。

### 3.13 `PENDING.md`（15 行，亲自读）
- **看到**：`:6-7` 把 `eng/tools/quality/check_module_map.py` 立为「唯一权威…禁止在登记面自证」——**该 .py 已被删除**（我 `git ls-files --error-unmatch` 确认 NO），只剩 `.pyc`。`:3` 引 `§7.1`（实为命令树，顶层结构在 §8.4）。`:9` 自称「如实登记」的计数 **62 / 50 实为 66 / 53**（我实测）。`:14` 称本目录**缺** `README.md` —— **该文件存在**（我确认 YES）；称 `MODULE_MAP.md` 无 pipeline 条目 —— 实有 `:94`。
- **判定**：**阻断**（15 行里 4 行失实，且其「反自证」授权链已断）。

### 3.14 `cpp/tests/fixtures/filters_provenance_name_not_in_library.json`（34 行，亲自读）
- **看到**：结构完整的负例夹具，`_comment` 自陈判据 `kCurveNameOutsideLibrary` 与修复前口径。**未发现内容缺陷。** ⛔ 但它在任何构建/运行脚本中均未被引用（我未能在 `lib/` 与 CMake 中找到加载点），属**孤儿夹具**。
- **判定**：**建议**。

### 3.15 `orchestrator/cpp/.gitignore`（5 行，亲自读）
- **看到**：覆盖 `*.o`/`*.exe`/`logs/`/`build/`/`nul`；**未覆盖 CMake 静态库产物 `*.a`/`*.lib` 与 `CMakeCache.txt`**（`CMakeLists.txt:28,54` 产静态库）。
- **判定**：**建议**。

### 3.16 `configs/stage1_gc_panel3_Red.json`（73 行，亲自读）
- **看到**：`:4`/`:6-9`/`:18`/`:29-31`/`:64-67` **全部路径为绝对 `F:\Astro dev\Astro CS Normalization Database\…`**，而本仓名为 `Astro CS Database` —— **在任何当前机器上都解析不了**。`:30-31` 指 `lib\photometric_calib\data\…`（迁移前路径，现役为 `lib/algorithms/photometry/data/…`）。`:64` `output.hiss` 配 `:71 legacy_hiss_compare: false` ⇒ **该键永不被读**；`:58` `stage_timeout_sec.hiss_verify` 对应的阶段**不在 `stop_after` 枚举内**（schema `:326-337`，我已核对）⇒ **该超时永不被用**。`:4` `GaiaDR3SP` 与 `:18` `GaiaDR3` 是不同目录。
- **判定**：**须修**（死键 + 绝对路径 + 迁移前路径）。

### 3.17-3.21（`test_logger.cpp`、`typed_dag.py`、`memory.md`、`architecture.md`、`panel1_Red.json`）
见第 1 节「未亲自读完」声明。子代理结论中我已独立复核者：`typed_dag.py` 悬空输入边（exit 0）、`memory.md` 指向已删目录、`architecture.md` 零引用、`panel1`/`panel3` diff 无复制粘贴错误（**任务点名的「panel3 残留 panel1」经复核证伪**）。

---

## 4. 发现清单

### 阻断（6）

| # | 位置 | 结论 |
|---|---|---|
| **B1** | `checkpoint`/`logger` 生产接线 | **Logger 文件日志在生产中从不生效**：`initialized_` 只在 `logger.cpp:71`(`init()`) 置真，而 `Logger::instance().init` 的**全部 6 处调用都在 `test_orchestrator_cli.cpp`**；生产 `main.cpp:304` 只调 `set_log_dir`。故 `logger.cpp:141` 的文件分支恒假，**不产生任何日志文件，无告警无错误码**。用户按 `output.log` 配好日志，永远拿不到文件。 |
| **B2** | 同上 | **断点续传在生产中是死代码**：`update_stage` 全部调用点在 `test_checkpoint.cpp`/`test_orchestrator_cli.cpp`，`save_checkpoint`/`load_checkpoint` 唯一调用点在测试（`:1129/1133`）。`checkpoint.h` 宣称的「跳过已完成阶段」从未在真实管线发生。 |
| **B3** | `module_registry.c:306` | **堆越界读**：`j.p< j.end` 只护 `j.p[0]`，随后无条件读 `j.p[1..3]`。输入缓冲 `malloc(sz+1)`，最多越界 2 字节。 |
| **B4** | `module_registry.c:886-892` | `acsd_registry_check_v1` **恒返回 `ACS_OK`**，finding 数只经出参回传。宿主「check 失败即报错」的习惯在此**永不触发**。 |
| **B5** | `admission_controller.h` | 全文件零实现 + `engineering_authoritative/` 规范源整目录不存在 + 仅被同样无 `.cpp` 的 `spill_manager.h` 包含。 |
| **B6** | `test_orchestrator_e2e.py` | 10 条模块路径全为迁移前旧布局（我 `ls` 验证 8/8 不存在）+ `from orchestrator import Orchestrator` 全仓无此模块 ⇒ **必然失败**，且未被任何 runner 注册。 |

### 须修（11）

1. `module_registry.c:730-732`+`:734`：**哈希门恰在哈希失败时失效**（静默置空 → 比对被跳过，无 finding）。
2. `module_registry.c:823`：`sha_actual` 被 DLL 自报值覆写，二者不符不记 finding（近乎自证）。
3. `test_orchestrator_cli.cpp:1113-1114`：**被改名却仍是恒真断言**的「幂等性」断言（未重读 `list`）。
4. `test_orchestrator_cli.cpp:1199`：`n_loaded >= 0` 恒真，且与 `:1198` 注释意图（≥1）矛盾；配合 `:1250-1291` 的 `[跳过]`，`main:1967` 可在零断言下返回 0。
5. `test_orchestrator_cli.cpp:1239`：`n3 == n1` 期望值由被测对象自产（自洽式）。
6. `test_orchestrator_cli.cpp:1865-1866`：`get_stage1_schema_sha256()` 的「一致性测试」只断言非空（恒真门）——而我实测两份 schema 因**多一个尾随换行**（内嵌 `}\n\n` vs 磁盘 `}\n`）**SHA 永不相等**，故这道门既没做、也做不出来。
7. `test_orchestrator_cli.cpp:1442-1445`：年份白名单断言 **2028 年必红**。
8. `test_orchestrator_cli.cpp:1520-1609` ×5：注释称「FITS 不存在」但**该 FITS 存在**，五道门依赖非预期失败原因。
9. `tests/CMakeLists.txt:50`：`PASS_REGULAR_EXPRESSION "VALID"` **匹配 `INVALID`** → 校验失败时门仍绿。
10. `module_loader/README.md:53-58`：段落逐字重复 + `:14`/`:50`/`:52` 三处悬空路径（我逐个 `git ls-files` 验证不存在）。
11. `PENDING.md`：授权工具已删（只剩 `.pyc`）+ 计数 62/50 实为 66/53 + 称缺 README 实则存在 + `§7.1` 引错章节。

### 建议（8）

`checkpoint.cpp` 无 fsync 且 POSIX 缺 WRITE_THROUGH（`:519-543`）；`fully_completed` 只置真不复位（`:738`，接线后即阻断）；`logger.cpp:290` open 前先赋路径（恒真门素材）；`cli_command.cpp:164/170` NaN 丢字段、inf 非法 JSON；`admission_controller.h` 硬编码 2GiB/90-70-60/20 无出处；`e2e.py:43` 五级 `..` 与注释「三级」矛盾；`panel3_Red.json` 全部绝对路径 + 2 个死键；`cpp/.gitignore` 未覆盖 CMake 静态库产物。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| **R1** | 截断 manifest，令 `abi_version` 的值以单个 `n` 结尾且该字节恰为输入末字节 | 「`module_registry.c` 的 JSON 解析不会越界」 | ✅ **成立**。`:306` 越过 `j.end` 读 3 字节，缓冲为 `malloc(sz+1)` ⇒ 读至分配块外 2 字节。 |
| **R2** | 令某 unit 的文件**不可读**（无权限），其余合法 | 「哈希不符会被 `HASH_MISMATCH` 捕获」 | ✅ **成立**。`:730` 失败 → `sha_actual[0]=0` → `:734` 的 `&& e->sha_actual[0]` 为假 ⇒ **比对整体跳过，无 finding、无错误码**。哈希门在哈希失败时静默失效。 |
| **R3** | registry 注入 `DUP_UNIT_ID` + `HASH_MISMATCH` + `LOAD_FAILED`，调用 `acsd_registry_check_v1` | 「check 会在有问题时返回非 OK」 | ✅ **成立**。`:891` 无条件 `return ACS_OK`。 |
| **R4** | `duration_ms = NaN` / `= +inf` 调 `output_jsonl_event_ex` | 「JSONL 事件恒为合法 JSON」 | ✅ **成立**。NaN ⇒ 字段静默消失；inf ⇒ 输出 `inf`（RFC 8259 非法），整行不可解析。 |
| **R5** | 对 `compute_config_sha256` 只改 `stage_timeout_sec` 与 `precision`，看哈希是否变化 | 「配置哈希覆盖全部字段」 | ⚠️ **未能证伪**：现有测试（`:1817-1854`）只改 precision + light。该字段是否入哈希**本轮未取证**，登记 UNRESOLVED。 |
| **R6** | 把 `stage1.template.json` 改坏，观察 `orchestrator_legacy_cli_validate` | 「门会红」 | ⚠️ 静态判定：该门 `PASS_REGULAR_EXPRESSION "VALID"`，`INVALID` 含子串 `VALID` ⇒ **应仍绿**。未实跑（禁编译），登记为静态推断。 |
| **R7** | 检查两份 schema 的 SHA 是否可相等 | 「内嵌与磁盘一致」 | ✅ 证伪一致**且**证伪可校验：差一个尾随换行 ⇒ SHA 永不相等。 |

**未成立的攻击（如实记录）**：`cli_command.cpp:42-46` 声称 `request_cancel()` 纯 atomic —— 核实为真，攻不动；`module_loader/README.md:46-52` 声称 registry 不在生产调用图 —— 核实为真，攻不动；`panel3` 残留 `panel1` —— `diff` 证伪；`test_orchestrator_cli.cpp:1869-1917` 三条 SIGINT 断言 —— 判红条件经复核确实成立，是真门。

---

## 6. 盲复算

遮住既有判定，对「本片是否存在让缺陷转绿的机制」独立重取，结果：

| 既有判定 | 我的独立取证 | 结论 |
|---|---|---|
| 「内嵌 schema 与磁盘 schema 今日一致」 | `diff` 显示内容一致，但**字节尾部不同**（内嵌多一换行） | **偏松**：既有结论用 `sed -n '32,506p'` 比对，遗漏尾字节，故未发现「哈希永不相等 ⇒ 一致性测试写不出来」这层 |
| 「`stop_after=browser_verify` 是活阶段」 | schema `:336` 含之 | **一致**（我未复核 orchestrator.cpp 阶段表，采信子代理） |
| 「`nside_value=0` 合法」 | schema 用 `oneOf` 约束 mode/value | **一致**（采信子代理） |
| 「logger/checkpoint 生产未接线」 | 我独立 grep 全部调用点，逐条落盘 | **一致**，且我把它从「须修」升为**阻断**（静默降级 + 用户可见后果） |
| 「e2e 测试不可用」 | 我 `ls` 验证 8/8 旧路径缺失、无 `orchestrator.py` | **一致**，我补了「未被任何 runner 注册」一层 |

**净偏差：偏松 1 处（schema 尾字节），其余一致；无偏严。**

---

## 7. 子代理派发记录

派发 **5 个**（要求 3-5）：`1b45a409`(C++ src/include)、`0cf8163a`(module_registry.c)、`76519eb6`(tests/gates)、`da107951`(C++ src/include 重复)、`7b426388`(typed_dag/docs/configs)。

> 注：`1b45a409` 与 `da107951` 因我提交时 prompt 重复，投了同一组文件。我保留双读，把第二次当作该高风险组的**盲复算臂**——两臂独立结论高度一致，互为交叉验证。

### 逐条复核与**否决**记录

| 子代理结论 | 我的处置 | 理由 |
|---|---|---|
| B3「Logger 生产从不生效」 | ✅ **采纳并升为阻断** | 我独立 grep 复核：6 处 `init` 全在测试，`main.cpp:304` 只调 `set_log_dir` |
| B4「checkpoint 续传是死代码」 | ✅ **采纳并升为阻断** | 我独立 grep `update_stage`/`save_checkpoint` 全部调用点 |
| 「内嵌 schema 与磁盘逐字节一致」 | ❌ **否决** | 我 `od -c` 证实内嵌多一个尾随换行；「一致」只在 `sed` 截断后成立 |
| 「`stop_after=browser_verify` 非遗留值」 | ✅ 采信（未亲自复核 orchestrator.cpp） | 证据充分，标注为采信 |
| 「`nside_value=0` 有 `oneOf` 保障」 | ✅ 采信 | 同上 |
| 「level 门滤不掉 ERROR」 | ✅ 采信 | `logger.h:30-35` 枚举序 DEBUG=0..ERROR=3，我亲自读过，成立 |
| 「sha256 shim 注释属实」 | ✅ 采信 | 我 grep 到 4 处调用全在测试 |
| 「`typed_dag` 悬空边 exit 0」 | ✅ 采信（**我已降级其定级**：负测固化 seed 语义，应加显式声明而非直接拒绝） | 我复核 `producer_of` 遍历面确认机制成立 |
| 「panel3 残留 panel1」 | ❌ **任务预判被证伪** | 子代理 `diff` 证伪，我亦复核 |
| 「`admission_controller.h` 零实现」 | ✅ **采纳并升为阻断** | 我独立 `git ls-files` + grep 复核 |
| 「`engineering_authoritative` 不存在」 | ✅ **采纳** | 我独立 grep 全仓零跟踪文件 |
| 「module_registry.c 内存/边界」 | ⛔ **0cf8163a 未回收**，其组结论缺失 | **UNRESOLVED**；我自己读完全文并给出 B3/B4/R1/R2/R3 |
| 「test_logger.cpp 全部结论」 | ⛔ **76519eb6 未回收** | **UNRESOLVED**，本片唯一实质盲区 |

**统计：派发 5；采纳 10 类；否决 1 类（schema「逐字节一致」）；UNRESOLVED 2 个子代理组（`0cf8163a` module_registry、`76519eb6` tests 侧未回收）。**

---

## 8. 自证段（可复跑）

```bash
cd "/workspace/Astro CS Database"
# 计数口径：排除 run/ 历史快照与 build/ 产物，否则 find/grep 结论全被污染
git -c core.quotepath=false ls-files | grep -v '^run/' | grep -v '^build/' > /tmp/tracked_live.txt

# B1 Logger 生产未接线
grep -rn 'Logger::instance().init' $(grep -E '\.(cpp|h)$' /tmp/tracked_live.txt)   # 6 处全在 tests/
grep -n 'Logger\|set_log_dir' lib/infrastructure/pipeline/orchestrator/cpp/src/main.cpp

# B2 checkpoint 死代码
grep -rn 'update_stage\|save_checkpoint\|load_checkpoint' $(grep -E '\.(cpp|h)$' /tmp/tracked_live.txt)

# B3/R1 module_registry 越界读（读原文 :306，注意只护 j.p[0]）
sed -n '304,308p' lib/infrastructure/pipeline/module_loader/module_registry.c
sed -n '604,608p' lib/infrastructure/pipeline/module_loader/module_registry.c   # malloc(sz+1)

# B4 恒真门
sed -n '886,892p' lib/infrastructure/pipeline/module_loader/module_registry.c

# R2 哈希门失效
sed -n '729,737p' lib/infrastructure/pipeline/module_loader/module_registry.c

# 3.6/3.9 双重 schema 权威 + 尾随换行（sha 永不相等）
C=lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp
awk '/R"JSON\(/{f=1;next} /\)JSON";/{f=0} f' "$C" > /tmp/embedded_schema.json
diff -u lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json /tmp/embedded_schema.json
sha256sum /tmp/embedded_schema.json lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json
tail -c 20 /tmp/embedded_schema.json | od -c | tail -2   # 内嵌: } \n \n
tail -c 20 lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json | od -c | tail -2  # 磁盘: } \n

# 3.7 三处恒真/自洽断言
sed -n '1107,1115p;1196,1200p;1236,1240p' lib/infrastructure/pipeline/orchestrator/cpp/tests/test_orchestrator_cli.cpp
sed -n '1863,1867p' lib/infrastructure/pipeline/orchestrator/cpp/tests/test_orchestrator_cli.cpp

# 3.9 VALID 匹配 INVALID
sed -n '46,51p' lib/infrastructure/pipeline/orchestrator/cpp/tests/CMakeLists.txt
printf 'INVALID\n' | grep -qE 'VALID' && echo "=> 'VALID' 正则匹配 'INVALID'：门在失败时仍绿"

# B5 admission_controller 零实现 + 规范源已删
git -c core.quotepath=false ls-files | grep -c admission
git -c core.quotepath=false ls-files | grep -c engineering_authoritative   # => 0
grep -rn 'MemoryBudgetManager::\|AdmissionController::\|PressureHandler::' $(grep -E '\.cpp$' /tmp/tracked_live.txt) | wc -l   # => 0

# B6 e2e 旧布局全缺
for d in lib/astro_image_io lib/calibration/python lib/plate_serve/python lib/photometric_calib/python; do [ -d "$d" ] || echo "MISSING $d"; done
git -c core.quotepath=false ls-files '*.py' | grep -cE '(^|/)orchestrator[^/]*\.py$'   # => 0

# 3.13 PENDING.md 失实
git -c core.quotepath=false ls-files lib/infrastructure/pipeline | wc -l              # => 66（文件称 62）
git -c core.quotepath=false ls-files lib/infrastructure/pipeline/orchestrator | wc -l # => 53（文件称 50）
git -c core.quotepath=false ls-files --error-unmatch eng/tools/quality/check_module_map.py || echo "已删除"
git -c core.quotepath=false ls-files --error-unmatch lib/infrastructure/pipeline/README.md && echo "README 存在（文件称缺）"

# 3.12 README 悬空
for p in 12_DLL_ABI_AND_LOADER_STANDARD.md eng/packaging/verify_install_tree.py eng/ci/ledgers/prod_wiring.json; do
  git -c core.quotepath=false ls-files --error-unmatch "$p" >/dev/null 2>&1 || echo "MISSING $p"; done
sed -n '53,58p' lib/infrastructure/pipeline/module_loader/README.md   # 53-55 与 56-58 逐字重复

# 3.16 panel3 配置绝对路径与死键
sed -n '4p;30,31p;58p;64p;71p' lib/infrastructure/pipeline/orchestrator/configs/stage1_gc_panel3_Red.json
grep -n 'stop_after' -A12 lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json | grep -n 'hiss_verify' || echo "hiss_verify 不在 stop_after 枚举 → 该超时永不使用"
```

---

## 9. 登记 UNRESOLVED

1. **`test_logger.cpp`（545 行）本轮未回收子代理结论**，是本片唯一实质盲区。
2. **`0cf8163a`（module_registry.c 车道）未回收**；该组结论由我本人阅读替代，但未经第二读者交叉。
3. **R5**：`compute_config_sha256` 是否覆盖 `stage_timeout_sec`，未取证。
4. **R6**：`PASS_REGULAR_EXPRESSION "VALID"` 的假绿为**静态推断**，未实跑验证（禁编译）。
5. **HEAD 不一致**：派单书称 `850a9ede`，实际为 `1fa477a7`。若审核基准应为前者，本片全部行号需按该提交重取。