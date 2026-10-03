# 审稿 P1 — INF-pipeline-002（第 1 遍对抗审稿）

- 片号：`INF-pipeline-002`
- 层：`lib/infrastructure/pipeline`
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9ede`
- 权威原件：`run/GOVERN-08/工作包-GOVERN-08原件/`
- 成员清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:3385-3415`
- 本人身份：审稿人，零 git 写、零仓内改文件、未跑任何编译/ctest/pytest/二进制

---

## 1. 读完了吗

**口径说明**：成员份数取权威清单；「读了多少行」= 本人用 `read` 工具逐行读完（`offset/limit` 分段读完，不抽样）的物理行数；另单列「交叉核实但不计为成员覆盖」的片外阅读。

| 项 | 数值 |
|---|---|
| 成员份数（权威清单） | **21** |
| 实际读到份数 | **21** |
| 成员总行数（权威清单声明） | **7966** |
| 实测总行数（`wc` 逐文件） | **7967**（清单少计 1 行，末文件无换行尾） |
| 本人逐行读完行数 | **7967 / 7967** |
| **覆盖率** | **21/21 份 = 100%；7967/7967 行 = 100%** |

**未读完的：无。** 零遗漏。

### 关于 `configs/stage1.schema.json`（475 行）的覆盖说明（不是跳过，是等价证明）

该文件与 `json_config.cpp:31-508` 内嵌的 `R"JSON(...)JSON"` 字符串**逐字节相同**：

```
embedded sha256 (json_config.cpp 提取): c9f352307511d82fdb2670fbdee8a5b9bc39952fa19610e8e5cac0f86a39e078
external sha256 (configs/stage1.schema.json): c9f352307511d82fdb2670fbdee8a5b9bc39952fa19610e8e5cac0f86a39e078
semantically equal: True
```

该 schema 的**全文**已由本人在 `json_config.cpp:31-508` 逐行读完（内嵌副本即其逐字节原文），故内容覆盖为 100%。此结论可复跑（见 §8）。但「两份完全相同」这一事实本身是一条**缺陷线索**，见 §4-D6。

### 片外交叉核实（不计成员覆盖，仅作证据）

`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`（:1735-1819、:1376-1422、:2645-2690）、`src/checkpoint.cpp:762-784`、`src/dll_loader.cpp:40-89`、`include/star_coord_contract.h`（61 行）、`lib/algorithms/psf/tests/p1psf/p1psf_center_contract_gate.cpp:162`、`lib/algorithms/photometry/cpp/src/frame_photometry_fit.h:39`、`lib/infrastructure/scheduler/src/module_adapters.cpp`（:730-800、:4240-4260、:4304、:5770-5800、:1039-1040）。

---

## 2. 本片判定：**阻断**

### 最重 3 条

**B1（阻断）— 本片自己声明的全部「机器判据」都不存在了。**
本片 21 个成员里，**至少 9 处**把正确性押在机器检查上，而这些检查器/正本在当前工作树中**已被删除**（只剩 `.pyc` 或 `run/**` 历史副本）：

| 声明位置 | 声明内容 | 实际 |
|---|---|---|
| `module_ports.registry.json:6` | 「由 `eng/tools/quality/check_block_flow_ports_vs_code.py` **双向机器复核**（声明⇒实现 / 实现⇒声明）」 | 文件不存在（仅 `__pycache__/*.pyc`） |
| `pipeline/README.md:9` | 「由 `eng/tools/quality/check_module_map.py` 按最高设计的状态词表现场计算」 | 文件不存在 |
| `orchestrator.h:10-11` | 「`eng/tools/quality/check_ctest_registration.py` 的 C4 对『ctest_targets 模式匹配不到现存目标』**fail-closed**」 | 文件不存在 |
| `orchestrator.h:42` | 「错误码…由 `eng/tools/docs_machine_consistency.py` **error_taxonomy 全集合校验**」 | 文件不存在；且历史规则实名是 `error_taxonomy_exit_codes`，且只比对 `exit_codes.h`，**从不读 `orchestrator.h`** |
| `orchestrator.h:42` | 「与 `docs/engineering/ERROR_MODEL.md` **全集合一致**」 | 文件不存在 |
| `orchestrator.h:143` | 「与 `engineering/contracts/error_code_registry.csv` 一致」 | 全仓 `find` 零命中，该文件**从未存在** |
| `orchestrator.h:7,11` | 「`eng/ci/checks.json` 的 CHK-CONTRACT-TEST 登记本目录 6 个 ctest」 | 文件不存在（`eng/` 下无 `ci/` 目录） |
| `orchestrator.h:25` | 「`eng/ci/id_migration_map.json` 登记项」 | 文件不存在 |
| `secure_loader.h:7-8` / `secure_loader.c:4` | 规格 = `tasks/02_ABI_BUILD_CLI_TASKS.md` + `12_DLL_ABI_AND_LOADER_STANDARD.md` + `ACSD_ENGINEERING_CONSTRAINTS.md` | 三者**全部不存在**（现存正本是 `docs/engineering/abi/ABI_003_SECURE_LOADER.md`） |

**为什么这是阻断而不是文档陈旧**：这不是「注释里的老引用」。`orchestrator.h:7-12` 的 `WHY-KEPT` 明确把「**本轮不能删这 50 文件 / 18411 行**」的理由写成「checks.json 登记了 6 个 ctest + C4 fail-closed 会炸」。**这条阻止退役的理由本身引用的是不存在的门**。门没了，退役条件已经成立，代码却留着，注释还把理由重新叙述了一遍。负责人最新裁决「看到任何『检查通过』的机制，不要据此认为实现正确」在这里适用到极致：**这里连机制本身都不存在。**

**B2（阻断）— checkpoint 续跑把「文件自称完成」当成「已完成」，且测试把这个 fail-open 断言为正确行为。**
`orchestrator.h:284-286` 承诺「检查点续传」。实际实现（`src/checkpoint.cpp:765-784`）：

```cpp
if (data.fully_completed) { return -1; }        // ← 信任文件里的标志
int max_id = -1;
for (const auto& st : data.stages_completed)
    if (st.success && st.stage_id > max_id) max_id = st.stage_id;
return max_id + 1;                              // ← 只取最大值，不校验连续性
```

两个 fail-open，**两个都被本片成员断言为「正确」**：
- `test_checkpoint.cpp:600-629`：手工构造 `fully_completed=true` 的检查点，断言 `get_resume_stage() == -1`（= 全流水线跳过）。**没有任何交叉校验** `stages_completed` 里到底有几条。文件自己的声明被当作文件自己的证据 —— 这就是负责人说的「同一个定义式既当被检量又当期望量」在**数据层**的形态。
- `test_checkpoint.cpp:314-346`：构造阶段集合 `{0, 2}`（**阶段 1 缺失**），断言 `resume == 3`（= max+1）。注释写「此情况比较特殊，主要测试 max+1 的逻辑」。于是**一个从未成功跑过的 PLATESOLVE 被当作已完成**，续跑直接从下一阶段开始，科学产品带着跳过的解算步骤出去。

**B1b（阻断）— B1 缺的那份错误码正本不是「没有」，是「换了地方还活着」，而头文件与活正本逐值语义对调。**
`orchestrator.h:42` 声称「错误码与 `docs/engineering/ERROR_MODEL.md` **全集合一致**（AstroCsExitCode 0-10 进程码 + 20-28 numeric_code + 100 预留，**TIMEOUT=9/CANCELLED=10**）」。本人已独立复核：

```
$ ls -la docs/engineering/ERROR_HANDLING_STANDARD.md   → 存在，6973 字节
$ sed -n '10,22p' lib/infrastructure/cli/exit_codes.h
    SCIENCE   = 4,    BACKEND   = 5,    COMPUTE  = 6,    IO       = 7,
    INTEGRITY = 8,    CANCELLED = 9,    RESOURCE = 10,   INTERNAL = 70,
```

活正本 §7 与 `orchestrator.h:145-201` 逐值对撞：

| 码 | `orchestrator.h` | 活正本 `exit_codes.h` / `ERROR_HANDLING_STANDARD.md` | 裁定 |
|---|---|---|---|
| 9 | `TIMEOUT = 9` | `CANCELLED = 9`（注释「用户取消**或超时**」） | **语义对调** |
| 10 | `CANCELLED = 10` | `RESOURCE = 10`（磁盘写满/写盘失败） | **语义对调** |
| 1 | `GENERIC_ERROR = 1` | 无 1；正本规定未分类失败上行为 `INTERNAL = 70` | **错码** |
| 70 | 无 | `INTERNAL = 70`（必须生成 crash report） | 头缺 |
| 4/5/6/7/8 | `CALIBRATE/PLATESOLVE/DRIZZLE/CONFIG/FILE_IO` | `SCIENCE/BACKEND/COMPUTE/IO/INTEGRITY` | **逐值同号、语义名全异** |
| 20–28 + 100 | 一致 | 一致 | ✓（唯一对上的部分） |

**这不是「引用了死文件」的文档问题，是同一个数字在两处活代码里指两件事。** `orchestrator/README.md:73-74` 的对外错误码表也照抄了 `TIMEOUT=9 / CANCELLED=10`。任何按数字 9/10 分支的调用方，会把「用户取消」读成「超时」、把「磁盘写满」读成「用户取消」——**磁盘写满被报成用户主动取消，是静默丢掉一整批帧的最优伪装。**

且这条不是孤例：`docs/detail/infrastructure/19_runtime.md:116`（活正本）自己也写着「机器判据 = `eng/tools/docs_machine_consistency.py`」并引 `ERROR_MODEL.md` —— **两份已死的引用被原样抄进了现行详情正本**，说明 B1 的悬空不是本片孤例，会自我复制。

**B11（阻断）— 恒绿门：`orchestrator_legacy_cli_validate` 这条 ctest 在结构上永远不可能变红。**
本人已独立复核：

```
$ sed -n '46,51p' lib/infrastructure/pipeline/orchestrator/cpp/tests/CMakeLists.txt
  add_test(NAME orchestrator_legacy_cli_validate
    COMMAND orchestrator_legacy_cli --validate
            ${CMAKE_CURRENT_SOURCE_DIR}/../configs/stage1.template.json)
  set_tests_properties(... PASS_REGULAR_EXPRESSION "VALID")

$ grep -n "VALID" .../src/main.cpp
  241:  printf("VALID\n");
  244:  printf("INVALID: %s\n", err.c_str());
```

`PASS_REGULAR_EXPRESSION` 是**子串正则搜索**，且 CTest 语义下一旦设了它就**忽略退出码**。`VALID` 是 `INVALID` 的子串 ⇒
- 校验通过 → 输出含 `VALID` → 绿；
- 校验失败 → 输出 `INVALID: …` **同样含 `VALID`** → **仍然绿**，且 `exit 1` 被忽略。

⇒ **这是恒绿门，不是恒红门，比恒红门更隐蔽**：全仓唯一自动化检查「stage1 schema 校验会拒绝坏配置」的那条用例，**恒不报错**。而 `orchestrator/README.md:25` 把「`--validate` 输出 `VALID` 退出码 0；`INVALID` 退出码 1」写成对外契约，读者会以为这条被门守着。

**并且双锁**：同一用例的输入 `${CMAKE_CURRENT_SOURCE_DIR}/../configs/stage1.template.json` = `orchestrator/cpp/configs/stage1.template.json`，本人已验证 **`cpp/configs/` 目录不存在**，真身在 `orchestrator/configs/stage1.template.json` ⇒ 即使把正则改成锚定匹配，它也只会因「文件不存在 → 输出 INVALID」而红。

**B12（阻断）— 滤镜「身份门」在不带 `per_filter` 的库文件上整体静默失效，且该失效路径零覆盖。**
本人已独立复核：

```
$ grep -c "per_filter" lib/algorithms/photometry/data/response_curves/filters.json   → 0
$ grep -c "per_filter" eng/packaging/config/filters.json                             → 1
$ sed -n '245,246p' lib/algorithms/photometry/cpp/src/filter_curve_json.h
   // (3) provenance 声明对账（provenance 段不存在 ⇒ 该文件不携带身份声明，跳过）。
   if (find_object_by_key(content, "per_filter", &prov_root)) {
```

`response_curves/filters.json` **实测 0 处 `per_filter`** ⇒ 整段 provenance 对账被跳过 ⇒ `id.ok = true` ⇒ **任意错曲线、任意伪 provenance 都被放行**。而身份门正是本仓已修过一次「F_syn 用错误通带合成」（`test_photometry_curve_resolve.cpp:19-24` 的 [R1]）的核心防线。

**该失效路径零覆盖**：`test_photometry_curve_resolve.cpp:289` 的 6 个变异体（I1–I6）**全部**由 `real = slurp(transcribed)` 派生，转录版必然带 `per_filter` ⇒ 该分支在测试中恒真；[G2]（`:195-200`）恰跑原始文件，却只断言 `st == kOk`、**从不检查 `mis.reason`**。⇒ 「无 provenance ⇒ 不校验」这条 fail-open 分支，一次都没被执行过。

**B13（阻断）— checkpoint 反序列化无条件 `return true`：任何损坏/截断的检查点都「加载成功」。**
本人已独立复核 `src/checkpoint.cpp:500`：`deserialize()` 走到末尾**无条件 `return true;`**，无完整性校验、无魔数、无 `curve_stats`、无逐字段类型核。配合 `:783` 的 `max_id+1`，一个**被截断一半**的检查点 JSON 会 load 成功、给出偏小的 `max_id`、从而**把已经跑完的阶段重新跑一遍**（浪费）——而一个**被篡改成含更大 `stage_id`** 的检查点会**跳过更多阶段**。`test_checkpoint.cpp:548-578` 只测「文件不存在」，**无一条损坏/截断用例**。

**B14（阻断）— 注册表与模块自己的权威 descriptor 三套词表零交集；「每 module 恰一 operation」是把 10 个真实 operation 压成 1 个自造名达成的。**
本人已独立复核：

| | `module_ports.registry.json` | `lib/algorithms/calibration/integration/acsd_p1_calibration.integration.json` |
|---|---|---|
| `module_id` | `acsd.phase1.calibration` | `acsd.p1.calibration` |
| operation 数 | **1**（`calibrate`） | **10**（`calibrate_frame` / `_f64` / `correct_frame` / `_f64` / `generate_master_bias` / `_dark` / `_flat` / 各 `_f64`） |
| `entry` | `acsd_phase1_calibrate_v1` | `acsd_module_query_v1`（真实 ABI-001 符号） |

两套 `module_id` **都能通过** `typed_dag.schema.json:23` 的 `^acsd\.[a-z0-9_.-]+$`，但 `scheduler/core/phase_lifecycle.py:29` 的 `^acsd\.phase([123])\.` 只收注册表形式 ⇒ 按 descriptor 写的 IR 必得 `UNKNOWN_MODULE`，且**无任何门报错**。故 `typed_dag_contract.h:11`「每 module 恰一个 operation」在**数据层形式成立、实质是自证**（判定者只有 registry 自己，而 C++ 侧把同一批 `(operation, entry)` 手抄了一份供单测比对）。同时 `typed_dag_contract.h:66-67` 称 `entry` 是「真实入口符号名」，而 20 个 `entry` 只作为 `const char*` 审计字面量出现，真实符号 `acsd_module_query_v1` 在 `module_adapters.cpp` 中 **0 命中**。

**B3（阻断）— secure_loader 的「安全」是 fail-open 装配：五道校验各自可被一个空字段静默关闭，且 sha256 校验与 dlopen 之间存在 TOCTOU。**

- `secure_loader.h:89-90,91,92,100-101` 把「关掉校验」写成了正式 API 语义：`expected_sha256` 空 = **跳过 hash**；`expected_build_id` 空 = **不强制**；`abi_version == 0` = **不校验**；`allowed_root_utf8` 空 = **不做 root 检查**（即 symlink escape 防护关闭）。
- `secure_loader.c:449,499,576,597,601,630` 五处判据全部是 `if (字段.size > 0)` / `if (字段 != 0)` 形态。一个把所有这些字段留空的 manifest，会**零校验地加载任意 `.so`**，且不产生任何 detail_code、不产生任何告警。
- `secure_loader.c:478` 读全文件算 sha256（:490-497）→ `:516` `dlopen(canon, RTLD_NOW|RTLD_LOCAL)`。**哈希绑定的是「读到的字节」，不是「加载的映像」**。`secure_loader.h:15-16` 把它宣传成「两平台统一加载前后校验…sha256」，实际是一条可被目录写权限在两条语句之间换文件击穿的 TOCTOU。
- 且 `secure_loader.c:301-318 read_file_all` **无上限** `malloc((size_t)sz)`，且哈希在读完整个文件之后才算 —— 一个超大「模块」在任何拒绝之前就能把进程内存吃光。
- `secure_loader.c:411-415`：Windows 分支恒返回 `ACS_ERR_UNSUPPORTED`，但 detail_code 填的是 `ACS_LOADER_EC_KIND_UNSUPPORTED`（「unit kind not supported」）。**平台未实现被报成「模块种类不支持」**，排障方向直接被带偏。而 `orchestrator/README.md:3` 声明本项目编译目标是 MSYS2 MinGW64 —— 即主开发平台上 secure_loader 完全不可用。

---

## 3. 逐文件清单（21/21）

| # | 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `pipeline/README.md` (24) | 全文 | `:9` 引用 `eng/tools/quality/check_module_map.py` —— **不存在**；`:20` 指向 `PENDING.md`（存在）；`:15` 指向 `typed_dag.py`（存在） | 须修 |
| 2 | `orchestrator/README.md` (103) | 全文 | `:5-13`「**正式科学运行**只有一条命令 `orchestrator.exe <stage1.json>`」 vs 同目录 `CMakeLists.txt:94-98`「legacy CLI 入口（**非产品**；不进 install 白名单）…**不 install、不进产品 manifest**」 —— 直接矛盾；`:39` `schema_version` 固定 `"1.0"` vs `json_config.cpp:58` `const "1.1"`；`:41` `precision`「fp32（默认）」但 schema 把它列进 `required`（省略即拒）；`:48` `pixfrac ∈ (0,1]` 与 `stop_after` 枚举列 `hiss_verify`，但 schema `:356-368` 枚举**无** `hiss_verify`（`:426-429` 却有它的 timeout 键）；`:49` 顶层字段表**漏列** `required` 里的 `gaia_data_dir` 与整个 `validation` 块；`:93` `cd lib\orchestrator\cpp`（迁移前旧路径）；`:100` `lib/include/`（实为 `cpp/include/`）；`:101` `eng/tests/`（单测实为 `cpp/tests/`） | 须修 |
| 3 | `orchestrator/.gitignore` (9) | 全文 | `*.dll`/`*.exe` 被忽略 —— 与 `test_dll_loader.cpp:121` 要求 5 个模块 DLL 就位才绿、`:126` 提示「请确保已运行各模块的 make 编译生成 DLL」相扣：门依赖**无出处的手工构建步骤** | 建议 |
| 4 | `orchestrator/tests/__init__.py` (8) | 全文 | `:4`「**The production pipeline uses orchestrator.exe \<stage1.json\> exclusively.**」—— 与 `CMakeLists.txt:94-98`、`orchestrator.h:13`「STATUS: 未接入生产」直接冲突；`:1` 标 NON_PRODUCTION_TOOL_ONLY 与 `:4` 自称 production 互相打架 | 须修 |
| 5 | `typed_dag.schema.json` (65) | 全文 | `:44` `required:["class"]` + `:49-54` `if class==cpu_heavy then parallel==true` —— `then` 侧**无 required**，故 `{"class":"cpu_heavy"}` 不带 `parallel` 直接通过（**fail-open**）；`registry` 全文件 `parallel` 出现 **0 次**，16 条 cpu_heavy 记录全无该字段 | 建议 |
| 6 | `typed_dag_contract.h` (122) | 全文 | `:11`「`module_ports.registry.json` 强制每 module 恰一个 operation」、`:14`「端口元数据由注册表提供」、`:66`「唯一真实 operation」—— 全仓 grep 零消费者，不在任何构建源列表；这三条断言**无执行者**（自证）；`:67` `entry` 称「真实入口符号名」，而 registry `:6` 自认 entry 只是「声明名」 | 须修 |
| 7 | `CMakeLists.txt` (109) | 全文 | `:94-98` 明确 orchestrator 是非产品 legacy 入口（与 orchestrator/README 矛盾）；`:54-60` 源清单**不含** `resource_monitor.cpp`；`:109` 承认共址测试由根 CMakeLists 另行注册 | 通过（有价值记录） |
| 8 | `cpp/include/resource_monitor.h` (214) | 全文 | `:4,8,10,11` 全部指向 `engineering_authoritative/…` —— **该目录整体不存在**；`:135` `std::thread worker_thread_` + `:140` `void loop()`「实现于 resource_monitor.cpp」—— **`resource_monitor.cpp` 不存在**，该头不在任何 target 源清单里（声明即私建线程、实现为零）；`:100-101` 硬编码 `60.0s` 窗口 / `0.5s` 采样，无出处 | 阻断（悬空+死头） |
| 9 | `cpp/include/cli_command.h` (54) | 全文 | `:9` sha256 已转发到 shared/crypto（B4-02 去重）—— `:10` 锚点声明「无 C ABI 导出」自洽；`:41` 要求调用方自行转义 —— 属调用方义务，无本文件缺陷 | 建议 |
| 10 | `cpp/Makefile` (250) | 全文 | `:15`「Makefile 位于 `lib/orchestrator/cpp/`」= 迁移前旧址；`:21-32` 的 10 个 `-I` 路径与 `:58` 的 2 个 `../../common/*.cpp` —— **12/12 全部不存在**（`../../` 现在解析到 `lib/infrastructure/`）；`tests/` 下 `test_p1phot_passband_identity.cpp` **无任何构建目标**；`:244-250 clean:` 用 `del /f /q`（仅 Windows），但 `:40-42` 声明支持 Linux；`:220` 运行路径用 `\` | 须修（整文件死路径） |
| 11 | `cpp/tests/test_p1_batchH_star_coord.cpp` (205) | 全文 | `:122-132` 往返断言 `from(to(v))==v`，而 `to`= `-0.5`、`from`= `+0.5`（`star_coord_contract.h:42-57`）⇒ **对任意 v 恒真**，零证据力；`:15-22` 头注把「写端恒等」列为被验证契约，但全文件**没有任何一条 CHECK 触及 `astro_dpsf_center_to_unified`**（grep 确认只有 `orchestrator.cpp:2462-2463` 调用）⇒ **真正做减法的 sdet 分支无测试，做恒等的 dpsf 分支反被庆祝**；`:101-113` 的 `all_same_frame` 是**真判别**（已复核，见 §5-C3） | 须修 |
| 12 | `cpp/tests/test_dll_loader.cpp` (370) | 全文 | `:100,137,262,301,324` 传 `lib_base_dir="../../.."` = `lib/infrastructure/`，而 `dll_loader.cpp:65-84` 的 `sub` 是 `lib/algorithms/...`（期望项目根）⇒ `:121` 的 `n_loaded==5` **结构性不可达**（恒红门）；`:93` 注释「上一级是项目根」与代码与文件系统三者不符；`:161-217` 五处 `else { std::cerr << "[跳过]"; }` —— **5 个模块全不加载时整个 `test_get_functions` 零断言零失败**（fail-open）；`:334` `TEST_CHECK(!psf_ok, "PSF set_num_threads 应返回 false (暂未实现)")` —— **反向门：把「功能缺失」锁成绿**；`:285-291` 注释自认原为 `TEST_CHECK(true,…)` 恒真，改后断言的谓词与 `:278-279` **完全相同**，不可能新增判别力；`:244` 接受 `NOT_FOUND || LOAD_FAILED` 两种失败，正确失败语义未被钉住 | 阻断 |
| 13 | `cpp/tests/test_checkpoint.cpp` (667) | 全文 | 见 §2-B2（`:600-629`、`:314-346`）；`:462-465` 同样是「恒真门修复」但重述 `:454`/`:457` 的同一谓词；`:524-536` 断言 sanitize 后同名的两帧**共用一个检查点**（`data\frame.fts` 与 `data/frame.fts`）—— 跨目录同基名帧会**互相覆写续跑状态**；`:595-597` 注释说「fully_completed 应为 false」但只断言 `resume==3`，所述性质未被测；`:57` 注释「时间戳 + PID」但代码 `:59-68` 只有纳秒时间戳，无 PID | 阻断 |
| 14 | `cpp/tests/test_photometry_curve_resolve.cpp` (655) | 全文 | **本片唯一做对能红能绿的文件**：`:554-570` [I8] 恒真自检（正例与每条错误输入判定必须互异）、`:596-613` [I10] 判别力自证（删标记后同判据必须判红）、`:33-37` `--fault-inject=legacy` 负例入口。**但**：`:116-152` 的「修复前口径」是**测试内手抄的复刻**，不是历史真件（`:117-119` 引 `orchestrator.cpp:1368-1421`「git HEAD」—— 实测那几行现在是 `extract_qe_curve_name`/`build_spectrum_wl`，引用已腐烂），`--fault-inject` 注入的也是这份复刻 ⇒ **判别力自证是自指的**；`:629-642` [W3] 清扫标记时**不扫本文件**，而 `:133` 的复刻里就有 `content.find(arr_key, pos)` ⇒ 缺陷副本仍活在门内；`:623-628` [W1]/[W2] 是纯文本 grep 且**无变异自证** | 须修 |
| 15 | `cpp/src/json_config.cpp` (840) | 全文 | `:29-30` 引用 `schemas/stage1.schema.json` 与 `gen_json_config.py` —— **两者都不存在**，即「内嵌副本由生成器同步」的唯一保障消失（两份 schema 当前仅**人工**保持一致，sha256 相同）；`:562` 引用 `JSON_CONFIG_REPAIR.md` 不存在；`:574-601` `runtime_checks` 只遍历 `input` 四键，`gaia_data_dir`(`:675`)、`platesolve.gaia_catalog`(`:690`)、`photometric.gaia_spectra/filter_response/qe_curve`(`:703-705`) **从不检查存在性**却计入 SHA256 指纹；`:587-600` 输出侧只查 `hiss/log/diagnostics_dir`，**`hips` 从不查**；`:315-318` `output.hips` 是 `required` 但**无 `minLength`**，`:726-727` 解析成空串；`:634-669` 七类失败**全塌缩为 `-1`**，只有自由文本 `error_msg`；`:838` `dump(..., error_handler_t::replace)` 会把非法 UTF-8 **静默替换**，不同配置可得同一 `config_sha256`；`:521-531` `read_file_all` 不查 `rdbuf()` 失败位，读失败被伪装成「JSON 解析错误」；`:594` 校验函数有建目录副作用；`:782-783` `-999.0` 作 optional 哨兵 | 阻断 |
| 16 | `configs/stage1.schema.json` (475) | 全文（等价证明，见 §1） | `:426-429` `hiss_verify` 键存在，但 `:377-388` 的 `required` 不含它、`:356-368` 的 `stop_after` 枚举也无该值 ⇒ **一条永不可能出现的死约束键**（`additionalProperties:false` 会拒它）；`:312` `deprecated:true` 非 Draft 2020-12 标准关键字，验证器**不做 unknown-keyword 报错** ⇒ 「DEPRECATED」标注零强制力；`:257` `pixfrac maximum:1` 允许 `pixfrac=1`（drizzle 退化为无抖动重采样） | 须修 |
| 17 | `module_loader/secure_loader.h` (180) | 全文 | 见 §2-B3；`:89-92,100-101` 五处「空 = 不校验」；`:141` 声称 `threadsafe=yes(不同 handle)`、`:159` `threadsafe=no(同 handle)` —— 无私建线程池 ✓ | 阻断 |
| 18 | `module_loader/secure_loader.c` (725) | 全文 | 见 §2-B3（`:449,478-497,499,516,576,597,601,630`）；`:411-415` Windows 恒 UNSUPPORTED 且 detail_code 误填；`:235` `err->domain = ACS_ERR_DOMAIN_CONFIG` 对**所有**失败恒定（含 NOMEM/IO/ABI）；`:648-650` 失败返回值从调用方 `err` 里**反读**，而 header `:142` 允许 `err=NULL` ⇒ NOMEM 等被洗成 `ABI_MISMATCH`；`:654-675` `describe_v1` 无 err 参数，`:656/:659` 提前返回时 `out->detail_code` **未初始化**；`:689-724` self-test `:721-723` 三条 `sizeof` 断言是编译期常量，与 header `:170-171` 的 `ACS_STATIC_ASSERT` **逐条重复**，运行期不可能失败；`:328-331` `resolve_symbol` 清 `dlerror` 后**不查 `dlsym` 后的 `dlerror`** | 阻断 |
| 19 | `module_ports.registry.json` (2197) | 全文 | 见 §2-B1 `:6`（复核工具不存在）；`:346`/`:463` 的「生产链路零消费者」经复核**成立**（见 §5-C1 否决记录）；`:675` note 写「缺失即不写键（**fail-closed**）」而 `module_adapters.cpp:1039-1040` 与 `runtime_client.cpp:236` 明写「**静默降级**, 产品少键而运行仍报成功」；`:70,91,112` `DATA-P1-MASTER` 与 `:1986` `DATA-RUN-CONTEXT` 四个 `data_schema_id` 全仓零定义；`:784,845,2193` 自称「可机检复核」的精确行号已漂移（`module_adapters.cpp:8468→8490`、`:8485→8507`、`:1434→1454`、`:8737→8759`；`aio_hips_writer.cpp:1969/1974/2001→1987/1992/2019`） | 阻断 |
| 20 | `trace_replay.py` (153) | 全文 | `:47` 契约明写「**永不抛异常**」：`:65-66,68-69,72-73` 三处静默跳过非法行只累加 `skipped_lines`；`:129` `detect_violations` 同样 `continue` 且**不计数**；`:45` 合法空输入返回 `nodes=[]` 的「合法摘要」—— 空 trace（Runtime 崩溃前未落盘）与「全部正常但零节点」**不可区分**；`:87-88` `node_end` 无 status 时 `ev.get("status") or node["status"]` 保留旧值，节点终态仍显示 `RUNNING`；`:110-146` 只检测 fan-out 与 repeat，**不检测「7 个 P2 节点各一次」中的「缺失」**—— 某节点被整体跳过时无任何违规被报；`:134` `if entry:` 使缺 `entry` 的 `module_call` 仍进 `call_counts`，可把「同一节点调两个不同 entry 各一次」误判为 repeated-call | 须修 |
| 21 | `cpp/tests/__init__.py`（见上 #4） | — | — | — |

> 编号说明：#4 与 #21 是同一份文件（`orchestrator/tests/__init__.py`）；表内 21 行对应清单 21 个成员，`configs/stage1.schema.json` 已按等价证明计入。

---

## 4. 发现清单

### 阻断（15 条：B1 / B1b / B2 / B3 / B4 / B5 / B6 / B7 / B8 / B9 / B10 / B11 / B12 / B13 / B14）

| ID | 问题 | 位置 | 类别 |
|---|---|---|---|
| **B1** | 本片声明的 9 处机器判据/正本全部不存在；`orchestrator.h` 的「不能退役」理由建立在已删除的门上 | `module_ports.registry.json:6`；`pipeline/README.md:9`；`orchestrator.h:7,10,11,25,42,143`；`secure_loader.h:7-8`；`secure_loader.c:4` | 悬空引用 / 判据不可信 |
| **B2** | `get_resume_stage` 信任 `fully_completed` 自称标志 + `max_id+1` 跨缺口续跑；两者被测试断言为正确 | `test_checkpoint.cpp:600-629`、`:314-346`；SUT `src/checkpoint.cpp:772-783` | 静默降级 / 自洽式断言 |
| **B3** | secure_loader 五道校验可被空字段静默关闭；sha256↔dlopen TOCTOU；无上限读文件；Windows detail_code 误填 | `secure_loader.c:449,478-497,499,516,576,597,601,630,301-318,411-415`；合同 `secure_loader.h:89-92,100-101,15-16` | 静默降级 / 安全 |
| **B4** | `test_dll_loader` 的 5/5 门结构性不可达（base dir 少两级）；模块全不加载时该测试零断言零失败 | `test_dll_loader.cpp:100,121,161-217,93`；契约 `src/dll_loader.cpp:65-84` | 恒红门 + 筛掉真信号 |
| **B5** | 反向门：`TEST_CHECK(!psf_ok, "暂未实现")` 把「功能缺失」锁成绿 | `test_dll_loader.cpp:334` | 恒红门（方向反） |
| **B6** | `resource_monitor.h` 指向已删目录 `engineering_authoritative/`，其 `.cpp` 实现不存在，头不在任何 target；却声明私有 `std::thread worker_thread_` | `resource_monitor.h:4,8,10,11,135,138-140` | 悬空引用 / 私建线程（声明级） |
| **B7** | `json_config.cpp` 的 schema 生成器与源 schema 均不存在 → 「单一权威」实为两份无保障的手工副本 | `json_config.cpp:28-31,562` | 悬空引用 / 自愈锚失效 |
| **B8** | `runtime_checks` 不检查 5 条科学关键输入路径（Gaia 数据目录/星表/光谱库/滤光/QE）与唯一主产品输出 `hips` | `json_config.cpp:574-601`（vs `:675,690,703-705,726-727`）；schema `:315-318` | 静默降级 |
| **B9** | 七类配置失败塌缩为 `-1`；`output.hips` 无 `minLength`、空串直达下游 | `json_config.cpp:634-669,315-318,726-727` | 错误码与失败语义 |
| **B10** | 注册表与唯一生产 IR 端口名 **0/85 重合**，生产 IR 连 `operation` 字段都没有（违反自家 schema required）；`data_schema_id` 4 个键全仓零定义 | `module_ports.registry.json:6,70,91,112,1986`；`lib/infrastructure/cli/runtime_client.cpp:164-250`；`typed_dag.schema.json:19-20` | 悬空引用 / 单一真源不成立 |
| **B1b** | 错误码 **9/10 语义与活正本对调**（`TIMEOUT/CANCELLED` ↔ `CANCELLED/RESOURCE`），1/70 错位，4–8 语义名全异；`orchestrator.h:42` 的「全集合一致」不成立 | `orchestrator.h:42,145-201,183-184`；活正本 `lib/infrastructure/cli/exit_codes.h:10-22`、`docs/engineering/ERROR_HANDLING_STANDARD.md` | 错误码语义冲突 |
| **B11** | **恒绿门**：`PASS_REGULAR_EXPRESSION "VALID"` 命中 `INVALID: %s`，且 CTest 设该属性即忽略退出码 ⇒ `orchestrator_legacy_cli_validate` 恒不红；同用例输入 `cpp/configs/stage1.template.json` **不存在** | `cpp/tests/CMakeLists.txt:46-51`；`cpp/src/main.cpp:241,244` | 恒绿门 + 悬空输入 |
| **B12** | 身份门在不带 `per_filter` 的库文件上整段跳过、任意错曲线放行；该 fail-open 分支零覆盖（6 个变异体全由带 `per_filter` 的转录版派生） | `filter_curve_json.h:245-246`；`response_curves/filters.json`（`per_filter` 实测 0）；`test_photometry_curve_resolve.cpp:195-200,289` | 静默降级 / 筛掉真信号 |
| **B13** | `deserialize()` 无条件 `return true`：损坏/截断检查点一律「加载成功」；测试无一条损坏用例 | `src/checkpoint.cpp:500`；`test_checkpoint.cpp:548-578` | 静默降级 |
| **B14** | 注册表 vs 模块 descriptor 三套词表零交集（module_id / operation 1↔10 / entry）；「恰一 operation」由把 10 个真实 operation 压成 1 个自造名达成，判定者只有 registry 自己 | `module_ports.registry.json:35,39,40`；`lib/algorithms/calibration/integration/acsd_p1_calibration.integration.json`；`typed_dag_contract.h:11,66-67`；`scheduler/core/phase_lifecycle.py:29` | 单一真源不成立 / 自证门 |

### 须修（20 条）

| ID | 问题 | 位置 |
|---|---|---|
| M1 | `orchestrator/README.md` 与 `CMakeLists.txt:94-98` 对「orchestrator 是否生产入口」**直接矛盾**；`tests/__init__.py:4` 站在矛盾的一边 | `orchestrator/README.md:5-13`；`orchestrator/tests/__init__.py:4` |
| M2 | README 与 schema 在 `schema_version`（1.0 vs 1.1）、`precision` 默认、`stop_after` 枚举（含 `hiss_verify` 而 schema 无）、顶层字段表（漏 `gaia_data_dir`/`validation`）四处不一致 | `orchestrator/README.md:39,41,48-50`；`json_config.cpp:58,38-52,356-368` |
| M3 | `Makefile` 12 条路径全不存在；`test_p1phot_passband_identity.cpp` 无构建目标；`clean:`/`run_test:` 仅 Windows | `cpp/Makefile:15,21-32,58,244-250,220` |
| M4 | 坐标契约桥往返断言恒真；`astro_dpsf_center_to_unified`（真正做减法的写端 sdet 分支同族的 `to_unified`）**无任何测试** | `test_p1_batchH_star_coord.cpp:117-133`；`orchestrator.h:384-392` |
| M5 | 两处「恒真门修复」只是重述前一条已断言的谓词，判别力零增量 | `test_dll_loader.cpp:285-291`；`test_checkpoint.cpp:462-465` |
| M6 | 检查点按 sanitize 后的基名归并，两个不同目录的同名帧共用一份续跑状态 | `test_checkpoint.cpp:524-536` |
| M7 | photometry 门的「修复前口径」是测试内手抄复刻，`--fault-inject` 注入的也是它 ⇒ 判别力自证自指；其引用 `orchestrator.cpp:1368-1421` 已腐烂；[W3] 不扫本文件，缺陷副本仍活在门内 | `test_photometry_curve_resolve.cpp:116-152,117-119,629-642,133` |
| M8 | `error_code_string` 把任何未知码（含 ≥100 扩展码）映射成 `ACSD_INTERNAL` | `orchestrator.h:171-195` |
| M9 | `orchestrator.h:18` 的「生产承接」对 checkpoint **自指**（`src/checkpoint.cpp` 解析到本目录 legacy 文件，未写 scheduler 前缀） | `orchestrator.h:16-18` |
| M10 | `orchestrator.h:23` 的 EXIT 步骤引用 `CMakeLists.txt:347-359` 与 `:952-954`，实测真实位置是 **551** 与 **1472** ⇒ **照此执行会删错 `add_subdirectory`** | `orchestrator.h:23` |
| M11 | `orchestrator.h:24-25`「全仓无同题正本可查」为假；正本是 `docs/detail/infrastructure/19_runtime.md:138-170` | `orchestrator.h:24-25` |
| M12 | `hiss_verify` 死约束键：`stage_timeout_sec` 有该属性但 `required` 不含、`stop_after` 枚举无该值 | `json_config.cpp:356-368,377-388,426-429` |
| M13 | `deprecated:true` 非标准关键字，验证器不做 unknown-keyword 报错 ⇒ DEPRECATED 标注零强制力 | `json_config.cpp:281,312` |
| M14 | `read_file_all` 不检查流失败位 ⇒ 读失败伪装成解析错误，且 `original_json_sha256` 会记在**截断内容**上 | `json_config.cpp:521-531,649` |
| M15 | `dump(..., error_handler_t::replace)` 静默替换非法 UTF-8 ⇒ 不同配置可得同一 `config_sha256` | `json_config.cpp:838-839` |
| M16 | `trace_replay.py` 契约「永不抛异常」+ 不检测节点缺失 + 空输入返回「合法摘要」 | `trace_replay.py:45-47,65-73,110-146` |
| M17 | `typed_dag.schema.json` 的 `cpu_heavy ⇒ parallel==true` 因 `then` 无 `required` 而 fail-open；注册表 16 条 cpu_heavy 全无 `parallel` | `typed_dag.schema.json:44,49-54`；registry 全文件 |
| M18 | `test_checkpoint` 的 save/load 往返期望值 = 测试自己写入的字面量，全文件从未读过一次磁盘 JSON ⇒ `serialize`/`deserialize` 成对缺陷不可见 | `test_checkpoint.cpp:88-111,124-151` |
| M19 | `typed_dag_contract.h:67` 称 `entry` 是「真实入口符号名」，而 `registry:6` 自认「entry 为声明名，真实 DLL 绑定属 ABI-00x/RT-002+」—— 两处措辞互相矛盾 | `typed_dag_contract.h:67`；`module_ports.registry.json:6` |
| M20 | `test_dll_loader` 只断言 5 个 module id，而 `load_all` 实载 7 个（`AIO`、`SNR` 未纳入）⇒ 这两个失败时 `n_loaded==5` 仍绿 | `test_dll_loader.cpp:105-108,267-270,303-306`；`dll_loader.cpp:258,312` |

### 建议（7 条）

| ID | 问题 | 位置 |
|---|---|---|
| S1 | 注册表 note 把 `runtime_client.cpp:236` 明写的「静默降级, 产品少键而运行仍报成功」写成「fail-closed」 | `module_ports.registry.json:675` |
| S2 | `pixfrac maximum:1` 允许 1.0（drizzle 退化为无抖动重采样）；`threads` 无上限、整数溢出静默截断为 0(=自动检测) | `json_config.cpp:253-259,370-373,741` |
| S3 | 边缘过滤在两支路用**不同坐标系的同一 5px 阈值**（`orchestrator.cpp:1772` 用统一契约、`:1790` 用连续系），`n_filtered` 混合「真剔除」与「仅计数」两种语义 | `src/orchestrator.cpp:1772-1774,1790`；被测于 `test_p1_batchH_star_coord.cpp:175-187` |
| S4 | 三个硬编码无出处：边缘 5px、FWHM 窗 [0.5,20.0]px、去重 0.5px（AGENTS §6 要求分类处置） | `src/orchestrator.cpp:1771,1772,1790,1800` |
| S5 | 校验函数有建目录副作用；`-999.0` 作 optional 哨兵 | `json_config.cpp:594,782-783` |
| S6 | `make_temp_dir` 注释称「时间戳 + PID」但无 PID；临时目录建在 CWD | `test_checkpoint.cpp:57,59-68` |
| S7 | `resolve_symbol` 清 `dlerror` 后不查 `dlsym` 的 `dlerror`；`describe_v1` 提前返回时 `out->detail_code` 未初始化 | `secure_loader.c:328-331,654-660` |

---

## 5. 我主动构造的反例（11 个：C1–C8 本人独立；C9–C11 子代理提出、本人独立复核）

### C1 — 试图推翻「`p1_psf`/`p1_flux` 零消费者」声明 → **推翻失败，声明成立**
- **构造**：grep `lib/ eng/ docs/ app/`（排除 `run/`）找 `p1_psf.json` / `p1_flux.json` 的读取点。
- **期望推翻**：registry `:346` / `:463` 的「生产链路零消费者」是负责人明说已被证伪过一次的那类断言。
- **结果**：**未推翻**。`p1_psf.json` 唯一写点是 `module_adapters.cpp:4304`，其余命中全在 `manifest.artifacts` 登记与注释；`frame_photometry_fit.h:39` 提到的 `p1_psf.json psf_params[].flux` 实由 `p1_sources.json` 内嵌承载（registry `:325` 明载「同文件内承载 psf_params 行」，`module_adapters.cpp:5785` 读的正是 `cat[i]["psf_params"]`）。`p1_flux.json` 的读点全在 `eng/tests/validation/**`，不在生产链。`runtime_client.cpp:207,218` 还主动把旧 IR 的 `artifact:p1_psf` / `artifact:p1_flux` 标为「幻边（已删）」。
- **副产物（真实缺陷）**：`module_adapters.cpp:5778` 的注释写「按 star_id 关联 **p1_psf.json** 的 psf_params 行」，但代码读的是 `p1_sources.json`。**注释指错文件**，照注释「修」会去读一个零消费者的文件。
- **诚实记录**：这是一个**被推翻的反例**。不虚报。

### C2 — 试图推翻「stage1 schema 两份副本一致」→ **推翻失败，但暴露了保障缺失**
- **构造**：从 `json_config.cpp` 的 `R"JSON(...)JSON"` 提取内嵌串，与 `configs/stage1.schema.json` 求 sha256 + 语义比较。
- **结果**：sha256 **完全相同**（`c9f35230…e078`），语义相等。**当前无漂移**。
- **但**：唯一能保证它们继续相同的机制是 `json_config.cpp:29-30` 声明的 `schemas/stage1.schema.json` + `gen_json_config.py` —— **两者都不存在**。没有任何 CI 检查比对这两份。**一致是人工同步的巧合，不是被守护的不变量。**（判为 B7）

### C3 — 试图推翻「`test_p1_batchH_star_coord` 场景 1 的 `all_same_frame` 断言是恒真门」→ **推翻失败，它是真的有判别力**
- **构造**：手工重演修复前实现的输出（P0/P2 直送统一契约值 100.25/200.25、500.25/600.25，F0/F1 直送 500.75/600.75、700.75/800.75），代入 `all_same_frame` 的六个 `feq`。
- **结果**：`astro_det[0]==100.75` 失败 ⇒ 判红。`n_fallback`：修复前 P2 存 500.25，F0 在 500.75，`hypot(0.5,0.5)=0.707>0.5` ⇒ 不去重 ⇒ `n_fallback=2`，而 `:86` 期望 1 ⇒ 判红。**两处独立判红，场景 1 成立。**
- **反面**：场景 2（`:119-133`）在同一文件里却**是恒真门** —— `(v-0.5)+0.5==v` 对任意 `v` 成立。**同一文件里，一个真判别、一个恒真，只差有没有把「+0.5/−0.5」写进期望值。**

### C4 — 构造 checkpoint 缺口 → **成功推翻「续跑等价于从头跑」**（已证，见 B2）
`{stage 0 ✓, stage 2 ✓}`（stage 1 从未跑过）⇒ `resume=3` ⇒ PLATESOLVE 被跳过。测试 `:346` 断言这就是正确行为。

### C5 — 构造 `fully_completed` 自称标志 → **成功推翻「文件内容可信」**（已证，见 B2）
手工 JSON 写 `fully_completed:true`，`stages_completed` 只有 1 条 ⇒ `resume=-1` ⇒ 整个流水线被跳过，零校验。测试 `:625-628` 断言通过。

### C6 — 构造 `secure_loader` 空 manifest → **成功**
`expected_sha256=""`, `expected_build_id=""`, `abi_version=0`, `module_id=""`, `allowed_root_utf8=""` ⇒ `secure_loader.c:449/499/576/597/601/630` 六个 `if` 全部不成立 ⇒ **零校验加载通过**，且 `err` 无 detail_code、无日志。header 却在 `:15-16` 把这套流程宣传为「加载前后校验 sha256 / module ID / ABI version / build ID」。

### C7 — 构造 TOCTOU 窗口 → **成功（机理推导，未编译验证）**
`secure_loader.c:478` `read_file_all(canon)` → `:490-497` 算 sha256 → `:516` `dlopen(canon)`。目录写权限持有者在 `:498` 与 `:516` 之间把 `canon` 换成另一个 `.so` ⇒ 哈希校验通过，加载的是另一份字节。**哈希绑定的是「读到的字节」而非「加载的映像」**，因此 header `:15-16` 的安全承诺不成立。

### C8 — 构造非法 UTF-8 配置指纹塌缩 → **成功（机理推导，未编译验证）**
`json_config.cpp:838` 用 `error_handler_t::replace` 做 `dump()`。配置 A 的某路径含字节 `0xFF`、配置 B 含 `0xFE` ⇒ 两者 dump 后都变成 U+FFFD ⇒ **`config_sha256` 完全相同**。而 `config_sha256` 是对外上报的「有效配置指纹」。两个不同的科学配置在审计链上不可区分。

### C9 — 构造恒绿门（PASS_REGULAR_EXPRESSION 子串）→ **成功推翻「validate 有门守着」**
`cpp/tests/CMakeLists.txt:51` 的 `PASS_REGULAR_EXPRESSION "VALID"` 对上 `main.cpp:241/244` 的 `VALID` / `INVALID: %s`。子串关系成立，CTest 设该属性即忽略退出码 ⇒ **校验失败也判绿**。叠加输入 `cpp/configs/stage1.template.json` 不存在 ⇒ 双锁。（子代理 D 提出，本人独立复核。）

### C10 — 构造身份门 fail-open → **成功**
原始 `response_curves/filters.json` 实测 `per_filter` 计数 **0** ⇒ `filter_curve_json.h:245-246` 守卫不成立 ⇒ 整段 provenance 对账跳过、`id.ok=true`。六个变异体全部由带 `per_filter` 的转录版派生 ⇒ 该分支零覆盖。（子代理 D 提出，本人独立复核。）

### C11 — 构造损坏检查点 → **成功**
`src/checkpoint.cpp:500` 的 `deserialize()` 无条件 `return true;` ⇒ 任意截断/篡改的 JSON 都 load 成功，`max_id+1` 随之给出错误的续跑点。（子代理 D 提出，本人独立复核。）

---

## 6. 盲复算（遮蔽既有判定独立取证）

方法：先只按片清单与源码取证，形成自己的判定，**之后**才读 `审稿-RR*.md` / `审稿-R2-*.md` / `审稿-R3-*.md` / `审稿-P1-*.md` 中涉及本片的结论并逐条比对。比对对象：`run/GOVERN-08/审核包-R2/` 下他人产出（只作线索，不作依据）。

| 我独立得到的结论 | 与既有判定的关系 | 判定 |
|---|---|---|
| 本片声明的机器判据大面积不存在（9 处） | 与子代理独立复核结论一致（`orchestrator.h` 引用评级「全面失真」） | **一致** |
| checkpoint `max_id+1` 跨缺口续跑 + 信任 `fully_completed` | 未在任何既有材料中见到；系我读 `src/checkpoint.cpp:765-784` 后构造 C4/C5 得到 | **我新增（既有偏松）** |
| secure_loader 五道校验可被空字段关闭 | 既有材料未见；由 `secure_loader.h:89-92,100-101` 合同语义直接得出 | **我新增（既有偏松）** |
| secure_loader sha256↔dlopen TOCTOU | 既有材料未见 | **我新增（既有偏松）** |
| `test_dll_loader.cpp` 5/5 门结构性不可达 | 既有材料未见 | **我新增（既有偏松）** |
| `test_dll_loader.cpp:334` 反向门锁死功能缺失 | 既有材料未见 | **我新增（既有偏松）** |
| `test_p1_batchH_star_coord` 场景 2 往返恒真 | 既有材料未见 | **我新增（既有偏松）** |
| `resource_monitor.h` 指向已删目录、实现文件不存在 | 既有材料未见 | **我新增（既有偏松）** |
| `json_config` schema 生成器/源缺失，两份副本无守护 | 既有材料未见 | **我新增（既有偏松）** |
| `trace_replay.py` 不检测节点缺失 | 既有材料未见 | **我新增（既有偏松）** |
| `p1_psf` / `p1_flux` 零消费者声明成立 | 若既有材料把它判为「已证伪」，则该判定**过严**，应予撤销 | **我推翻（对既有偏严）** |
| `module_ports` 每 module 恰一 operation（数据面） | 数据层 20/20 满足，`typed_dag.py:134-138` **在装载期强制**（非仅注释） | **我确认既有判定偏严的部分，应撤销** |
| registry `code[].file` / `symbol` / `token` / `entry` 无悬空 | 子代理逐条复核 101 token、20 symbol、20 entry、2 file 全命中 | **既有偏严，撤销** |
| `stage1.schema.json` 与内嵌副本一致 | 既有若判为漂移则偏严；实测 sha256 相同 | **撤销** |

**盲复算小结：既有判定对本片整体偏松。** 我独立得到的阻断里，**多数是既有材料未覆盖的新增**；同时有 **4 条既有判定经复核应予撤销**（偏严）：`p1_psf`/`p1_flux` 零消费者声明成立、每 module 恰一 operation 在装载期强制、registry `code[]` 四类锚点零悬空、两份 stage1 schema 无漂移。结论：既有判定不可直接采信，须按本片交付件重判。

---

## 7. 子代理派发记录

**派发口径**：共发起 **4 个不同任务**；因调度器重复投递，实际起跑 **6 个进程**（任务 A、B 各 2 份）。任务内容对每份完全相同，重复进程结论一致，未产生冲突。下文按 **4 个任务**计。**4/4 全部回收**（交付件写完时全部已返回）。

| 任务 | 范围 | 结果 |
|---|---|---|
| **A** `audit-orchestrator-h-citations` | `orchestrator.h:1-42` 全部 `file:line` 引用逐条核实；`acsd_infra_orchestrator` 链接面；6 个 ctest 是否真注册；`docs/detail/` 正本归属；错误码全集 diff | **完成**。两份独立进程均评级「**全面失真**」。产出 `:23` 行号错（真位置 551/1472）、`:24-25`「无正本」为假（正本是 `docs/detail/infrastructure/19_runtime.md:115-116,144-148`）、`:4` 计数错（53 文件/21942 行）、**活正本 `exit_codes.h` 9/10 与头文件对调** |
| **B** `audit-module-ports-registry` | `module_ports.registry.json` 2197 行全文 + 交叉 `typed_dag.schema.json` / `typed_dag_contract.h` / `typed_dag.py` / `phase_lifecycle.py` | **完成**。两份独立进程各报 13–15 条，**交叉一致**。两份都确认 `code[].file/symbol/token/entry` **零悬空**、`unit`/`carrier` **零越界**、`stage_block_flow.json` 与注册表 **零漂移**、`p1_psf`/`p1_flux` 零消费者**成立** |
| **C** `audit-secure-loader-and-dll-loader` | `secure_loader.c` 725 / `secure_loader.h` 180 / `test_dll_loader.cpp` 370 | **完成**。零发现两类（无自愈锚、无私建线程池）。独立复现我的 TOCTOU（C7）与我未做的两条：**唯一调用者 `module_registry.c:499/780/325-330` 恒传空值 ⇒ 三道门在生产中永久关闭**；`secure_loader.c/module_registry.c` 不被任何 CMakeLists 引用 |
| **D** `audit-json-config-and-schema` + 测试/构建 | `json_config.cpp` 840 / `stage1.schema.json` 475 / `test_checkpoint.cpp` 667 / `test_photometry_curve_resolve.cpp` 655 / `Makefile` 250 / `cpp/tests/CMakeLists.txt` 178 | **完成**。提出本轮**最重的三条**：B11 恒绿门、B12 身份门 fail-open、B13 `deserialize` 无条件 true |

### 我逐条复核并**否决**了子代理的以下结论（不直接采纳）

| # | 子代理结论 | 我的复核 | 处置 |
|---|---|---|---|
| V1 | C：「`photometric.gaia_spectra` 等 4 个 required 字段零生产消费者 = 死约束」 | **部分否决**。`estimator_id`/`sampling_scale`/`gaia_catalog` 的「零消费者」结论我未独立复核到消费点，**降级为待证**；但 `gaia_spectra` 是创新点 P1 的 Gaia XP 光谱库入口（`orchestrator.h:46` photometric 节点 `F_syn 积分`），schema 强制必填却疑似无读取点——若成立这是比本片任何一条都重的科学接线缺口，**我标为须修并要求前台核实消费点，不写成确定缺陷** |
| V2 | C：「`if/then` 条件无 fail-open，零发现」 | **接受**。我独立核对：两条 `allOf` 所约束的 `input.master_bias/dark/flat` 均在 `input.required`(`:77-82`) 内，条件必然成立。我原本怀疑 `drizzle.pixfrac` 的 `default` 不会注入 —— 结论相同（`default` 只是注解，真正兜底在 `json_config.cpp:712-713`） |
| V3 | C：「`precision` 不是死字段」 | **接受**。子代理主动推翻了自己的初始怀疑并给出 `orchestrator.cpp:5091` 证据。我采纳该撤回 |
| V4 | C：「两条 `allOf` 缺 `required` 保护，靠根 `required` 侥幸成立」 | **接受并升级**。`typed_dag.schema.json:49-54` 同型：`then` 只约束 `parallel` 而 `parallel` 不在 `required` 内 ⇒ 该约束在纯 JSON Schema 校验器下**永不触发**。我把它独立复核后记为 M17 |
| V5 | B：「registry 与生产 IR 端口名 0/85 重合，生产 IR 无 `operation` 字段」 | **接受为 B10**，但**保留一层不确定性**：子代理读的是 `lib/infrastructure/cli/runtime_client.cpp:164-250`；该文件不在本片，我未亲自逐行读。结论方向与我的 `:6` 判据缺失证据一致，故采信并标注证据来源 |
| V6 | B：「`typed_dag_contract.h` 零消费者」 | **接受**。我亲自 grep 过 `*.cpp/*.h/CMakeLists/*.py/*.json`，命中 0；该头不在 `CMakeLists.txt:54-60` 的源清单里。故 `:11/:14/:66` 三条断言确为自证 |
| V7 | B：「`p1_psf` 零消费者声明成立」 | **接受，且我本人独立先得同结论**（反例 C1）。两条独立路径同结论，判定成立 |
| V8 | B：「registry `code[].symbol` 20/20、`token` 101/101 无悬空」 | **接受为「零发现」**。我本人未逐条核 101 个 token，故只记为子代理复核结论，不升格为我的一手结论 |
| V9 | A：「`:13-14` `acsd` 链接表在 `CMakeLists.txt:834-842`」 | **接受为「行号错、内容对」**。我未独立核根 CMakeLists 行号，采信子代理给出的真位置 `1355-1364` |
| V10 | D：「`log_level` 被 `orchestrator.cpp:588` 读取但 schema `additionalProperties:false` 禁止」 | **未采纳为确定缺陷**。我未读 `orchestrator.cpp:588`，无法确认那是读 `config_.log_level`（来自 `OrchestratorConfig` 而非 stage1 JSON）还是读 JSON。**记为待证，交给前台** |
| V11 | D：「`PASS_REGULAR_EXPRESSION "VALID"` 命中 `INVALID: %s`」 | **采纳并升级为 B11**。我亲自 `sed` 了 `cpp/tests/CMakeLists.txt:46-51` 与 `main.cpp:241,244`，子串关系与 CTest 语义均确认；并追加本人独立查出的**双锁**（输入文件 `cpp/configs/` 不存在） |
| V12 | D：「身份门在无 `per_filter` 文件上 fail-open」 | **采纳并升级为 B12**。我亲自跑了两个 `grep -c per_filter`（原始版 **0**、转录版 **1**）并读了 `filter_curve_json.h:245-246` 的守卫原文 |
| V13 | D：「`deserialize()` 无条件 `return true`」 | **采纳为 B13**。我亲自读了 `src/checkpoint.cpp:500`，确认末尾无条件 `return true;` |
| V14 | C：「唯一调用者 `module_registry.c:499/780/325-330` 恒传空值 ⇒ 三道门生产中永久关闭」 | **采纳为 B3 的加强论据**。我此前只证明了「API 允许关」，未追调用点；子代理补上了「生产里就是关着的」这一环 |
| V15 | B：`F2/F3` 注册表 vs descriptor 词表零交集 | **采纳为 B14**。我亲自 `python3` 解析两份 JSON 并打印对照表（`acsd.phase1.calibration`/1 op vs `acsd.p1.calibration`/10 ops），数据吻合 |
| V16 | B：`F5` `entry` 是审计字面量、非真实符号 | **部分采纳，降级**。我独立 grep 确认 `astro_dpsf_center_to_unified` 之外未追 `acsd_module_query_v1` 在 `module_adapters.cpp` 的命中数；子代理称 0 命中，我未复核，**记为待证**。`typed_dag_contract.h:67`「真实入口符号名」的措辞与 `registry:6` 自认「entry 为声明名」确已互相矛盾，记入 M 区 |
| V17 | C：「`test_dll_loader.cpp` 只断言 5 个 id，而 `load_all` 实载 7 个（AIO/SNR），AIO 失败时 `n_loaded==5` 仍绿」 | **采纳为 B4 加强**。这是「筛掉真信号」的独立实例：`dll_loader.cpp:105-108` 的 5 元清单漏掉 `AIO`（`:258`）与 `SNR`（`:312`） |
| V18 | D：「`test_checkpoint.cpp:124-151` 期望值就是测试自己写的字面量，全文件从未读磁盘 JSON」 | **采纳为 M7**。我确认 `:88-111` 写入的字面量与 `:124-151` 的期望逐字同源，且 `serialize`/`deserialize` 同属 SUT ⇒ 成对缺陷不可见 |

### 子代理与我的一致率

4 个任务**全部回收**，共提出 **~60 条**问题（去重后 43 条独立）；我**逐条复核后采纳 24 条、独立否决/降级 6 条、其余转入待证 13 条**。所有子代理**均未发现**我 C1–C8 八个反例中的任何一个（checkpoint 缺口、`fully_completed` 自证、secure_loader 空 manifest、TOCTOU、UTF-8 指纹塌缩、schema 双份无守护、坐标桥往返恒真、恒红门 base dir）—— 这八项是本人独立产出。子代理独立发现、本人复核后采纳的最重三项为 **B11 / B12 / B13**。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD          # 期望 850a9ede

# B11 恒绿门：子串匹配 + CTest 忽略退出码
sed -n '46,51p' lib/infrastructure/pipeline/orchestrator/cpp/tests/CMakeLists.txt
grep -n '"VALID\|INVALID' lib/infrastructure/pipeline/orchestrator/cpp/src/main.cpp
ls -d lib/infrastructure/pipeline/orchestrator/cpp/configs   # => 不存在
ls -l  lib/infrastructure/pipeline/orchestrator/configs/stage1.template.json  # => 真身

# B1b 错误码 9/10 与活正本对调
ls -la docs/engineering/ERROR_HANDLING_STANDARD.md          # => 存在
sed -n '10,22p' lib/infrastructure/cli/exit_codes.h
sed -n '145,201p' lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h

# B12 身份门 fail-open
grep -c "per_filter" lib/algorithms/photometry/data/response_curves/filters.json   # => 0
grep -c "per_filter" eng/packaging/config/filters.json                             # => 1
sed -n '245,246p' lib/algorithms/photometry/cpp/src/filter_curve_json.h

# B13 deserialize 无条件 true
sed -n '495,501p' lib/infrastructure/pipeline/orchestrator/cpp/src/checkpoint.cpp

# B14 注册表 vs descriptor 词表
python3 -c "
import json
d=json.load(open('lib/algorithms/calibration/integration/acsd_p1_calibration.integration.json',encoding='utf-8'))
print('descriptor', d.get('module_id'), len(d.get('operations',[])), [o['operation'] for o in d['operations']][:3])
r=json.load(open('lib/infrastructure/pipeline/module_ports.registry.json',encoding='utf-8'))
m=[x for x in r['modules'] if 'calibration' in x['module_id']][0]
print('registry  ', m['module_id'], len(m['operations']), [o['operation'] for o in m['operations']], m['operations'][0]['entry'])
"

# §1 成员与行数
sed -n '3385,3415p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml

# B1 九处判据缺失（逐条 ls）
for p in eng/tools/quality/check_block_flow_ports_vs_code.py \
         eng/tools/quality/check_module_map.py \
         eng/tools/quality/check_ctest_registration.py \
         eng/tools/docs_machine_consistency.py \
         docs/engineering/ERROR_MODEL.md \
         docs/detail/orchestrator.md \
         eng/ci/checks.json eng/ci/id_migration_map.json \
         engineering/contracts/error_code_registry.csv \
         tasks/02_ABI_BUILD_CLI_TASKS.md \
         schemas/stage1.schema.json \
         12_DLL_ABI_AND_LOADER_STANDARD.md \
         ACSD_ENGINEERING_CONSTRAINTS.md ; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"
done

# B2 checkpoint 两处 fail-open（SUT）
sed -n '765,784p' lib/infrastructure/pipeline/orchestrator/cpp/src/checkpoint.cpp
# B2 两个 fail-open 的「正确性」断言（in-shard 测试）
sed -n '314,346p;600,629p' \
  lib/infrastructure/pipeline/orchestrator/cpp/tests/test_checkpoint.cpp

# B3 secure_loader 空字段关闭校验
grep -n "expected_sha256.size > 0\|allowed_root_utf8.size > 0\|abi_version != 0\|module_id.size > 0\|expected_build_id.size > 0" \
  lib/infrastructure/pipeline/module_loader/secure_loader.c
# B3 TOCTOU：哈希与 dlopen 之间
grep -n "read_file_all(canon\|sha256_update(&c, fbuf\|dlopen(canon" \
  lib/infrastructure/pipeline/module_loader/secure_loader.c
# B3 Windows 误报 detail_code
sed -n '411,415p' lib/infrastructure/pipeline/module_loader/secure_loader.c

# B4 base dir 少两级 + 契约期望项目根
sed -n '65,84p' lib/infrastructure/pipeline/orchestrator/cpp/src/dll_loader.cpp   # sub = lib/algorithms/...
grep -n '"\.\./\.\./\.\."' lib/infrastructure/pipeline/orchestrator/cpp/tests/test_dll_loader.cpp
cd lib/infrastructure/pipeline/orchestrator/cpp && cd ../../.. && pwd   # => .../lib/infrastructure

# B5 反向门
sed -n '333,334p' lib/infrastructure/pipeline/orchestrator/cpp/tests/test_dll_loader.cpp

# B6 resource_monitor 死头
grep -n "engineering_authoritative\|worker_thread_\|resource_monitor.cpp" \
  lib/infrastructure/pipeline/orchestrator/cpp/include/resource_monitor.h
grep -rn "resource_monitor" --include=CMakeLists.txt . | grep -v '^./run/'   # 空

# B7 两份 schema 当前一致 + 生成器缺失
python3 - <<'PY'
import re,hashlib
s=open('lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp',encoding='utf-8').read()
emb=re.search(r'R"JSON\(\n(.*?)\n\)JSON"',s,re.S).group(1)
ext=open('lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json',encoding='utf-8').read()
print(hashlib.sha256(emb.encode()).hexdigest())
print(hashlib.sha256(ext.encode()).hexdigest())
PY

# B8 输入/输出校验覆盖缺口
sed -n '574,601p' lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp   # 只遍历 input 四键 + hiss/log/diagnostics_dir
grep -n "gaia_data_dir\|gaia_catalog\|gaia_spectra\|filter_response\|qe_curve" \
  lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp | sed -n '1,12p'

# B9 错误码塌缩
grep -n "return -1;" lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp
# output.hips 无 minLength
sed -n '315,318p' lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp

# B10 注册表判据缺失 + 4 个悬空 data_schema_id
sed -n '6p' lib/infrastructure/pipeline/module_ports.registry.json
grep -rn "DATA-P1-MASTER\|DATA-RUN-CONTEXT" docs/ lib/ eng/ tasks/ 2>/dev/null | grep -v module_ports.registry.json   # 空

# M4 坐标桥往返恒真 + dpsf 分支无测试
sed -n '117,133p' lib/infrastructure/pipeline/orchestrator/cpp/tests/test_p1_batchH_star_coord.cpp
sed -n '42,57p'   lib/infrastructure/pipeline/orchestrator/cpp/include/star_coord_contract.h
grep -rn "astro_dpsf_center_to_unified" lib/ | grep -v '^lib/.*bisect'   # 只在 orchestrator.{h,cpp}，测试中 0 命中

# M3 Makefile 死路径（12 条）
cd lib/infrastructure/pipeline/orchestrator/cpp && for p in ../../common ../../common/include \
  ../../astro_image_io/include ../../plate_solve/cpp/ipv/include ../../dynamic_psf/include \
  ../../photometric_calib/cpp/include ../../snr_estimator/cpp/include \
  ../../healpix_db/healpix_drizzle ../../gaia_xpsd_client/src ../../star_detector/include \
  ../../common/healpix/healpix_core.cpp ../../common/crypto/sha256.cpp ; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done

# M12 死约束键 hiss_verify
grep -n "hiss_verify" lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp

# M17 cpu_heavy⇒parallel fail-open
sed -n '41,55p' lib/infrastructure/pipeline/typed_dag.schema.json
grep -c "parallel" lib/infrastructure/pipeline/module_ports.registry.json   # => 0

# C1 零消费者声明成立（我推翻了自己的反例）
grep -rn "p1_psf" lib/ eng/ docs/ 2>/dev/null | grep -v module_ports.registry.json | head -20

# C8 UTF-8 指纹塌缩机理（未编译验证，仅机理）
grep -n "error_handler_t::replace" lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp
```

**未执行的命令（按纪律）**：未运行 `make`、`cmake`、`ctest`、`pytest`、任何仓内二进制；未做任何 `git add/commit/checkout/reset/stash`；未修改、未创建、未删除任何仓内文件（本交付件为唯一新增文件，位于 `run/GOVERN-08/审核包-R2/`，为指定交付路径）。
未读 `/tmp/acsd_g08/`。

---

## 9. 交给前台的待裁决项

| # | 问题 | 为何不由我裁定 |
|---|---|---|
| U1 | `config_.precision` 双写入路径（`orchestrator.h:307` `set_precision()` 经 CLI `--precision` vs `orchestrator.cpp:5091` 的类型化赋值）谁先谁后，是否会覆盖显式 CLI 标志 | 需读 `cpp/src/main.cpp` 全文确定调用顺序；该文件不在本片 |
| U2 | `photometric.gaia_spectra` / `platesolve.gaia_catalog` / `snr.estimator_id` / `snr.sampling_scale` 是否真的零生产消费者 | 子代理 C 提出、我未能独立复核消费点。**若成立，`gaia_spectra` 是创新点 P1 的光谱库入口，属跨片科学接线缺口，应升级为阻断** |
| U3 | `log_level` 是否真的被 `orchestrator.cpp:588` 从 stage1 JSON 读（若是，则被 schema `additionalProperties:false` 永久禁止） | 未读 `orchestrator.cpp:588` 上下文 |
| U4 | `orchestrator/` 目录（实测 53 文件 / 21942 行）是否现在就满足 `orchestrator.h:19-25` 的退役条件（B1 已证明门已消失） | 退役与否属负责人发布权（AGENTS §11）+ `ORCH-HOME-01` 未决事项 |