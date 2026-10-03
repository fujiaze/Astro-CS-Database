# 审稿-P1 · ENG-tests-007（G08-05 对抗审稿 第 1 遍）

- 仓：`/workspace/Astro CS Database`　HEAD=`850a9edefd47434b9ab71bc907c3de1e0814b323`
- 成员清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml` 第 **1436–1501** 行
- 本遍定义：对该片 58 份材料的一次完整重读。零 git 写、零仓内文件修改、零编译、零 ctest/pytest/零实验脚本、零 `run/GOVERN-08/工作包-GOVERN-08原件` 之外的口头依据。**未读 `/tmp/acsd_g08/`。**

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（权威版声明 / 我逐条解析列表） | 58 / 58 ✅ 一致 |
| 成员总行数（权威版 `实际行数`） | 10422 |
| 成员总行数（我按 `read` 逐份重算，含 4 份无尾换行文件） | **10426** |
| **亲自完整读完** | **37 份 / 7463 行** |
| **亲自部分读完** | **3 份 / 717 行** |
| **确认为空（0 行；0 行文件读完即读完）** | **10 份 / 0 行** |
| **未读** | **8 份 / 1898 行** |
| **行覆盖率** | **8180 / 10426 = 78.5%** |
| **份覆盖率（完整读完 + 确认为空）** | **47 / 58 = 81.0%** |

### 未读完的（如实列出）

| 文件 | 行数 | 状态 |
|---|---|---|
| `eng/tests/cpu/dispatch/cpu005_route_decision_test.cpp` | 366 | **完全未读** |
| `eng/tests/backend/phase2_fixture_main.cpp` | 318 | **完全未读** |
| `eng/tests/cli/test_unified_blocks_templates.py` | 292 | **完全未读** |
| `eng/tests/integration/p2_integrate/p2_pixel_weight_test.cpp` | 263 | **完全未读** |
| `eng/tests/cpu/avx512/provider_avx512_handshake_test.c` | 237 | **完全未读** |
| `eng/tests/system/runtime/runtime_event_stream_test.cpp` | 216 | **完全未读** |
| `eng/tests/unit/p1_hips_writer_test.cpp` | 205 | **完全未读** |
| `eng/tests/testkit/fixtures/demo_fixture.txt` | 0 行（**4 字节**，`od -c` = `t e s t`） | 按字节读完（0 行 = 无行可读） |
| `eng/tests/unit/core_scheduler_test.cpp` | 259 | 读了 1–209，**缺 210–259（50 行）** |
| `eng/tests/unit/p2_upm/oracle/anchors.json` | 458 | 读了 1–120 + 300–458，**缺 121–299（179 行）** |
| `eng/tests/unit/p1wcs/p1wcs_oracle.hpp` | 348 | 读了 1–120 + 240–348，**缺 121–239（119 行）** |

> ⚠️ 诚实声明：上列 8 份**完全未读**的材料，其结论在本交付件中**一律标注为「仅子代理取证、我未复核」**，不计入我本人的判定。三个部分读文件的三处缺口由 5 个子代理独立补齐（各自逐行读完），我在 §7 逐条复核后才采纳。

> 口径注：`wc -l` 与 `read` 的 4 行差额来自 4 份无尾换行文件（权威版 `实际行数` 10422 偏低）；`demo_fixture.txt` 权威版记 0 行但**并非空文件**（4 字节）—— 只看 `wc -l` 会误判为「空文件」，这是本片唯一一处「行数与实体形态脱节」的点。

---

## 2. 本片判定：**阻断**

最重的 3 条：

1. **`p2_seam_gate_test.cpp` 整个「接缝数值门」是与被测实现完全解耦的代数恒等式**（`p2_seam_gate_test.cpp:50-59`）。我亲自逐行推导：`:50-51` 构造 `A = v+5.0+star`、`B = v-3.0+star` ⇒ `A−B ≡ 8.0`（逐像素精确）；`:54` 手填 `C_B = -8.0`（注释 `:53` 写「求解:」，但**全文件无任何求解器**，这个 −8 是照着构造出的 8 手工取负）；`:56-57` 于是 `before ≡ 8.0`、`after ≡ 0.0`，`:58-59` 的 `|before−8|<1.5` 与 `|after|<0.5` **按定义必然成立**。更关键：`:25-39` 的 `overlap_med` 是本文件内的 `static` 函数，**不调用任何生产接缝/定标代码**；`:24` 注释「复用 P2-003 三块重叠语义」是**伪引**——点名 P2-003 为权威，实际内联了一份平行实现。资源门半边同样恒真：`:71` 把被测量设成 `compute_cores_threshold(g) * 1.2`，而生产 `lib/infrastructure/cli/resource_gate.h:365` 的通过条件是 `thr > 0 && g.avg_equivalent_cores < thr` ⇒ 该条件恒假 ⇒ `:74-75` 恒过。

2. **`test_common_abi.py` 整套 API-001 机器门已死，且唯一的「头编译试金石」编译的是测试自造的桩**（`test_common_abi.py:6`、`:8-15`、`:37-44`）。`DOC = docs/api/COMMON_ABI_V1.md`，而 `docs/api/` **整目录不存在**（文档已迁到 `docs/engineering/COMMON_ABI_V1.md`），`:20` 在 `setUpClass` 无条件 `open(DOC)` ⇒ 异常抛在任何断言之前 ⇒ test_01/02/03/05 **五个用例一次都没跑过**。叠加 `:8-15` 的 `HEADER` 是本文件自己写的 9 行字面量，`test_04_header_compiles_as_c_and_cpp` 把它写进临时 `.c` 交 gcc/g++，**生产头 `lib/include/acsd/abi/module_api_v1.h` 从未进过编译器** ⇒ 该门结构上不可能因真实缺陷翻红，只能因 gcc 缺失而红。

3. **`rt005_registry_test.cpp:153` 的守卫恒假，使文件头宣称的 P1-001 负向面成为死代码**。`std::string_view(id).substr(0, 15) == "acsd.phase1."`：`"acsd.phase1."` 是 **12** 字符，`substr(0,15)` 返回 15 字符（`"acsd.phase1.cal"`），`string_view::operator==` 先比长度 ⇒ **恒 false**。于是 `:154 CHECK(insp.failed())` 永不执行，`acsd.phase1.calibration` 落入 `:156 CHECK(insp.ok())`。文件头 `:6-7` 宣称的「Phase1=真实 operation 适配器: inspect 未执行显式拒绝」**从未被断言**。

---

## 3. 逐文件清单（58 份）

> 「读到什么 / 看到什么 / 判定」三栏中，标 🔒 的是**我本人逐行读完**的；标 ⛔ 的是**完全未读、结论仅来自子代理、我未复核**。

| # | 文件 | 读到什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `contracts/product_family/field_constraints_oracle.py` (1005) 🔒 | 独立结构 Oracle，29 道门 O01–O36，`semantic_gate_errors` 七 kind | O30/O31 有**正反对照**活性门（`:792-798` 明写「否则门恒真自证」），设计是对的；但 `:423` `all(k in ds or True ...)` 是**恒真子句**；`:442` 用模块级 `_REPO` 而非 `self.root`，`:12` 承诺的 overrides 遮蔽不了唯一读 `lib/` 的那道门 | **须修**（`:423` 恒真门、`:442` override 失效） |
| 2 | `conformance/echo/tests/unit/echo_host_callback_test.c` (850) 🔒 | 15 个测试段，host fake 全套 | 判红能力真实（`:674 g_log.calls==1`、`:821 alloc==free`、`:573 bytes_count==24` 精确算术）；但 `:640/672/702/730` 用 `ACSD_ECHO_ECODE_*`，`types.h:48-56` 定义的是 `ACS_ECHO_ECODE_*`（少一个 D） | **阻断**（编译不过；4 处未定义标识符） |
| 3 | `unit/p1_drz/p1_drz_test.cpp` (778) 🔒 | 闭合 fixture、units/oracle/negative/geometry 四组、`ACSD_DRZ_FAULT` 注入组 | **本片质量最高的一份**：负向组真造 4 类错误实现逐一断言门必红 + 正控制；geometry 组用 raw `a_jp` 独立重算（`:625-647`），容差分级有推导注释（`:605-607`）。小口子：`:619-622` rows 为空时闭合门零执行；`:345` mutant 是两种归一化的杂合体 | **通过**（附 2 条建议） |
| 4 | `unit/p2_sky/p2_sky_test.cpp` (593) 🔒 | 12 个测试函数，真调生产 `sky_plane.cpp` | fail-closed 段（`test_negative` / `test_dcgain`）有鉴别力；但接缝门是**往返自证**——`field_true` 在 `:404` 声明、`:420` 写入后**全文再无读取**（我 grep 确认仅 2 处命中）；`:234-238` `bias()` 在 eval 失败时 `return 0.0`，使 `:241` 变 `0 < 0.5*bu` 恒真；`:305-316` `rss_kb()` 读不到 statm 时 `return 0` ⇒ `:341` 恒真，`:342 (void)r0;(void)r1;` 是作者自己不信这个量的残留 | **须修**（自证 + 2 处恒绿逃逸口） |
| 5 | `abi/test_secure_loader.py` (518) 🔒 | 11 个负测 + 3 个正测 + L1/L2/L3 静态面 | 真实 ABI 拒绝面，负测逐错误码；**但 L3（`:456-459`）结构性恒绿**——探针失败分支（`abi003_loader_probe.c:99-100`）只打印 `status=%d detail=%u out_null=%d`，**从不打印 loader 消息串**，而消息串（`secure_loader.c:207-225`）才是路径/sha 唯一泄漏面；`:4` 引 `tasks/02_ABI_BUILD_CLI_TASKS.md`（`tasks/` 全仓不存在） | **须修**（L3 恒绿 + 伪引） |
| 6 | `unit/p2_upm/oracle/anchors.json` (458) 🔒部分 | 5 锚全矩阵 + 3 顶层阈值 | `:321 "kappa": Infinity` **非 RFC 8259 合法 JSON**；`:439` F_illcond kappa=2.92e9 超自家 `:455 kappa_max=1e6` 三个数量级；`:455 kappa_max` 已被 `p2_upm_ma_test.cpp:252` 判为**退役**却仍以活契约外观冻结；A/B 带 `als_vs_scipy_maxdiff ~3e-14`（ALS×SciPy **真交叉**，是本档唯一强项） | **阻断**（零消费者 + 非法 JSON，见 §5-A） |
| 7 | `io/test_hips_output_contract.py` (419) 🔒 | 10 个 TestCase，中断注入、路径穿越、并发、树哈希、交叉 oracle | 路径穿越/权限/并发是真门；但 `:330` 与 `:335` 是**同一方程 `f(x)==f(x)` 的两种写法**（`recompute_tree_hash` 实现就是 `tree_hash(entries)`，`doc["tree_hash"]` 正是同一函数写下的）；`:112-113` 注释声称「fitsverify 在 stage 临时文件上执行」但断言只是 `assertIn("signal", published_products())`——`SpyStoreIO` 只有 5 种事件（`write_bytes/copy_file/atomic_rename/unlink/remove_tree`），**无 fitsverify 事件**；`:359` 依赖的 `libfits_core_test.so` **未被 git 跟踪** | **须修**（往返自证 + 伪注释 + 归档未受控） |
| 8 | `unit/p2_rej/oracle_rej.py` (411) 🔒 | 常量块、`posterior`、`classify`、`calibration`、3+6 fixtures、`emit_inc` | 校准 fixture 是**专为检出阈值放宽设计的反例**（`:307` 注释明说）；但 `:69 noise_flags` 赋值后**全文再无读取**（我 grep 确认）⇒ 生产侧噪声标志缺陷结构上不可能翻红；`:44` 引的 `SO-07` 在 `docs/` 下零命中、实为被 14 条条款复用的签字人字段 | **须修**（死输入 + 追溯链指向错对象） |
| 9 | `unit/p2_rej/CMakeLists.txt` (30) 🔒 | 3 条 add_test + 依赖 | 3 个被引用路径存在；`:5`「控制器待集成提交」是**过期注释**（根 `CMakeLists.txt:1490` 早已 `add_subdirectory`）；`:11-12` `lib//algorithms` 双斜杠、`:18` 缺 `if(TARGET)` 守卫（与 `aio/CMakeLists.txt:32` 口径不一致） | **建议** |
| 10 | `unit/p2_seam_gate_test.cpp` (102) 🔒 | 4 段 + `overlap_med` | **教科书级代数恒等式恒真门**，见 §2 第 1 条。附带 `:38` 取 median 而非 max（天然容忍至多 50% 像素灾难性错误）、`:93` 是 `:59` 与 `:75` 的纯复述（零新增信息） | **阻断** |
| 11 | `unit/p1wcs/p1wcs_oracle.hpp` (348) 🔒部分 | 7 个 oracle 的推导注释与实现 | 每条都写明「与被测路径不同源」的理由，且 `oracle_gnomonic_vec`（`:64-86`）确为**真正独立的第三路径**（三维向量法）；`:290` 引 `ipv_wcs.cpp:463-464 AP[1,0]-=1` **已漂移**——我实测该处是 `uv_x/uv_y` 计算，真值在 **`:512-513`**（`AP[6] -= 1.0; // AP_10` / `BP[1] -= 1.0; // BP_01`） | **须修**（悬空行号引用） |
| 12 | `unit/p1wcs/p1wcs_tests_main.cpp` (21) 🔒 | 5 组注册表 | 只是注册表本体；**无 `g_pass==0` 的零执行守卫**（对比 `p1_drz_test.cpp:773-776` 有）⇒ 筛选词失配即可全绿 | **建议**（筛选逻辑在 `p1wcs_test_main.hpp`，不在本片） |
| 13 | `unit/p1_hips/adapter_entry_impl.cpp` (12) 🔒 | 一行 `#include` 生产源 | **不是桩，是真生产源**，这点是对的；`:4` 引作先例的 `eng/tests/unit/gaia_client.c` 已迁至 `lib/infrastructure/gaia_xpsd_client/src/` | **建议** |
| 14 | `config/fixtures/positive/mosaic_legacy_contract.phase_config.json` (18) 🔒 | 退役形态夹具 | 本身是**有效夹具**（mosaic/export schema 保留显式 legacy `else` 分支，schema 接受 / CLI 拒绝 rc=3，两层分工不矛盾），有活调用者 | **通过** |
| 15 | `backend/test_p1004_joint_gate.py` (198) 🔒 | 4 个用例 + fixture 自建 | **反向门**：`:180 assertEqual(res.get("run_id",""), "")` 断言 `resource_summary.json` 的 run_id **必须为空**——IMPL 一旦修好立刻转红；`.get(...,"")` 默认值使**键整个缺失也照样绿**，分不清「缺键」与「空串」。`:173` 先筛掉 `e.get("run_id")` 为空的**事件**再断言「全事件一致」，被证明的恰是被筛剩下的子集 | **阻断**（反向门） |
| 16 | `backend/test_bench_candidates.py` (108) 🔒 | 5 个测试 + 探针编译 | `:96 assertRegex(sha256(data).hexdigest(), r"^[0-9a-f]{64}$")` 是**恒真门**——任何字节串的 sha256 必然匹配，测的是 hashlib 性质，永不可能红；`:90` 只验 BEST 的**行形态**，从不从原始样本重算最小值 ⇒「是否挑了最好看的候选」完全未验 | **阻断**（恒真门） |
| 17 | `config/gate_assertions.py` (117) 🔒 | 5 道门 G1–G5 | G1/G2/G4 是真门（11 个负例逐个必败）；但 **G5 是反向极性**：`:102-104` 要求每条滤镜 provenance 都 `startswith("unverified")`——一旦有人真正核实某条来源并改写标注，G5 立刻转红，**惩罚改进**；`:106` 的「无零点列」用 `re.compile("zero", IGNORECASE)` 套在整篇 JSON 文本上找英文单词 `zero`，`:105` 套在键路径上 ⇒ 一个 `"wavelength_nm": 0.0` 的零点列完全不会被发现（**门名声称的属性 ≠ 门实际测量的量**） | **须修** |
| 18 | `cli/cli_test_hygiene.py` (42) 🔒 | 42 行，cwd/env 落点 | 本体极小，只做 cwd 重定向 + `ACSD_REPO` 声明，逻辑正确；但 `:5` 引 `eng/ci/checks.json::UT-CLI`（`eng/ci/` 整目录不存在）、`:32` 称「该变量是 `commands.cpp:600` 既有开关」（`getenv("ACSD_REPO")` 实为 753 行，600 行是 `filesystem::rename`） | **建议**（悬空引用） |
| 19 | `cli/cli_fixture.py` (185) 🔒 | 进程级 fixture 缓存 + fail-closed 异常 | **本片最规范的一份**：失败一律抛 `CliFixtureError` 且消息含 rc / 命令 / stderr 尾部，`:141` 明确「宿主无 g++（fail-closed）」，`:35` 明确「不新增 SKIP」。两个小口子：`:139 if _EXE and os.path.isfile(_EXE): return _EXE` **无源码指纹** ⇒ 改 8 个 TU 之后静默跑上一版；`:27-28,54,62` 引的 `eng/ci/prepare_linux_fixtures.py` / `run/ci/run-checks/...` / `run/CTESTFULL-01/` 均不存在 | **建议** |
| 20 | `api/test_common_abi.py` (51) 🔒 | 5 个用例 + 自造 `HEADER` | 见 §2 第 2 条：**整套门已死**（`docs/api/` 不存在）+ **判据读的是桩**（`:8-15` 自造 9 行头，生产头从未进编译器） | **阻断** |
| 21 | `version/test_version_consistency.py` (66) 🔒 | 3 个用例走活 `gen_version` | `:4-8` docstring 诚实自陈 test_04/05 等已随 G08-01 移除；`:16-38` 的自制 `validate_schema` 覆盖了 `version.schema.json` 用到的全部关键字。`:49` 已从硬编码版本改为单源派生（`:47-48` 记了这次修正）。**本体无缺陷** | **通过** |
| 22 | `unit/rt005_registry_test.cpp` (173) 🔒 | 5 个测试函数 | 见 §2 第 3 条 `:153` 恒假守卫。另：`:5` 声称「用 JSON 正确转义」但**全文件无 JSON parser**（只有 `idx.find(...)` 子串）；`:6,138` 声称「每个模块 validate_config」而 **validate_config 全文件从未被调用**；`:36-52` 只 create 10 个 id（`reg.size()==22`）⇒ 12/22 模块的 factory 从未被调用 | **阻断**（`:153`）+ 须修 3 条 |
| 23 | `unit/rt007_artifact_store_test.cpp` (167) 🔒 | 6 个测试函数 | **文件头与代码直接相反**：`:3` 声称「篡改 path/hash/unit/schema/换 producer → 硬失败」，但 `:90-96` 篡改 path 后断言的是 **成功**（`CHECK(s2.store(d).ok())`、`CHECK(s2.bind_p1_to_p2(...).ok())`）；`:106/:110/:113/:116` 四条「篡改」**全部由同一条 duplicate-id 规则满足** ⇒ 把 unit/schema/producer 校验整个删掉，四条照样全绿。`:140-152` roundtrip 只比 5/20 字段 | **阻断** |
| 24 | `unit/rt002_budget_test.cpp` (166) 🔒 | 6 个测试函数 | 预算门本身有牙（`:65 CHECK(outer.acquired())` 存在）；但 `:39` 与 `:98` 的 `if (!lease.acquired()) return;` **后无前置断言**（对比 `:65` 有）⇒ acquire 一律失败时 test_1（violated=false、peak=0、available==4）与 test_4 全绿。`:59-73` 号称「嵌套」实为同线程顺序两次获取；`:51 i == 0 ? 4 : 4` 是恒等三元 | **须修** |
| 25 | `unit/aio/CMakeLists.txt` (130) 🔒 | 全部 add_test / add_executable / set_tests_properties | 我逐个核实 17 个被引用路径**全部存在**，零悬空。`:32 if(TARGET acsd_platform_math)` 是好设计（`:27-31` 写清了独立 configure 时会退化成 `-lacsd_platform_math` 的原因）。`:41` 硬编码 `python3` 无配置项 | **通过** |
| 26 | `unit/aio/mutations/mutate_and_check.py` (151) 🔒 | 12 条 mutation + 正控制 + `build_and_run` | 12 条 `old` 锚点在当前 `product_io` 各命中恰好 1 次（`:124` 强制 `count != 1` → ANCHOR-MISS 计入 missed，**正确的 fail-closed**）；无豁免清单、无 try/except、无环境变量逃逸；`:121 copytree` 复制真源码树后真重编，不是桩。**但 `:98-102` 与 `:131-137` 存在计分不对称**：cmake configure 失败 → `None` → 计入 missed；**build 失败 → `return 1` → 落到 `:135 if rc != 0` 被打印成「CAUGHT」** ⇒ 变异源编译不过会被算成成功捕获 | **须修**（恒红伪装） |
| 27 | `unit/io_ownership_test.cpp` (138) 🔒 | 手写 FITS 夹具 + `--inject-failure` 回归锁 | `:108-112` 的 fail-closed 非空守卫（`:110` 明写「判据会退化为恒真」）是**本片最佳实践之一**。问题：`:2` 与 `:133` 引 `eng/tools/check_aio_ownership.py`——**已从活树删除，只剩 `__pycache__/check_aio_ownership.cpython-313.pyc`**（我实测 `ls` 报 No such file）；`:127 CHECK(rc != 0)` 无法区分「exit 1」「shell 没 exec 成（127<<8）」「信号打死」 | **须修**（悬空引用 + 回归锁过宽） |
| 28 | `unit/executor_provider_race_test.cpp` (76) 🔒 | 1 个测试函数，4 条 CHECK | `:2-3` 声称「TSAN 构建下运行；修复前 TSAN 必报 data race」，但 4 条 CHECK **无一条能检出数据竞争**——原始有竞态的实现在 x86 上照样全绿；`:62` 注释自认「STARTED 快照允许为空」，恰恰把竞态所在相位放行。TSAN 声称在默认门里不存在 | **须修**（无判别力 + 声称的判红手段缺席） |
| 29 | `system/runtime/runtime_event_stream_test.cpp` (216) 🔒部分 | 头部 A–E 五组判据声明（`:14-21`）+ `_WIN32` 分支（`:50-61`）+ POSIX 子进程探针（`:63-79`） | POSIX 侧判红能力扎实（`:21` 给出判红变异方案：把 `jsonl.h` 的 fputs/fflush 改回不检查返回值 ⇒ B/C/D 全红），是本片**唯一自带变异判红方案的判据**。但 `:50-61` 的 `_WIN32` 分支把 B/C/D/E 四组判据整体替换为 3 条**在健康发射器上必真**的检查（`:56 ok`、`:57 !write_failed()`、`:58 publication_exit_code(OK)==OK`）后直接返回；`:53` 注释自称「（不静默、不假装绿灯）」——**但在 Windows 构建上网络效果恰恰是一个假绿灯**（退出 0 且未验证任何写失败分类） | **须修**（我已亲自复核 `:50-61`；子代理另报 `:97` 的 `pub2` 在 B/C/D 三处未断言，我未复核） |
| 30 | `unit/core_scheduler_test.cpp` (259) 🔒部分 | 8 个测试函数 | `:187` 注释写「100/70 → 同时刻**至多 1 个**在途（无回压穿透）」，代码写 `CHECK(peak_used.load() <= 2)`；`:174 p = now/70`，2 个 70B 节点同时在途 ⇒ `now=140 ⇒ p=2 ⇒ 2<=2` **绿** ⇒ 该门只抓穿透 ≥3，抓不住它自称要抓的穿透 2。测试名 `no_busy_spin` 的 4 条 CHECK **无一测量自旋**。`:159 c_done_before_b` 依赖 C 线程在 A 置位后才进入 ⇒ 竞态门 | **须修**（上界错位一档） |
| 31 | `unit/p1_hips_writer_test.cpp` (205) ⛔ | 仅子代理取证 | 子代理称 `:108-112` 是正确的 fail-closed 守卫、`:177-183` 确在 dirA/dirB **独立构建两次**逐字节比对（非往返自证）；`:113-120` 逐字断言 FITS 80 列卡对 CFITSIO 版本极脆 | **建议**（我未复核） |
| 32 | `cpu/dispatch/cpu005_route_decision_test.cpp` (366) ⛔ | 仅子代理取证 | 子代理称真调生产 `decide_kernel_v1`/`build_route_table_v1`，逐 stage/逐 fallback_reason 子串断言有鉴别力；`:17-23` 用 `std::numeric_limits` 未 include `<limits>`；`:199-200` 断言写成或式子串，鉴别力被稀释 | **通过**（我未复核） |
| 33 | `cpu/avx512/provider_avx512_handshake_test.c` (237) ⛔ | 仅子代理取证 | 子代理称正测依赖真实 CPUID/XGETBV，但 `:125/152/177/198` 三个 `if (api)` 守卫下若 query 返非 OK 则静默跳过 5 段中的 3 段；`:198-226` 只探测索引 1/COUNT/999，**唯一注册的 index 0 内核从未被执行**，而 `:19-20` 注释声称验了「kernel 表自洽」 | **须修**（我未复核） |
| 34 | `unit/gaia_unshuffle_test.c` (78) 🔒 | 3 个测试函数 | `:29/:55` 的期望数组是**按列主序语义手工独立推导**的，与实现无共享代码路径 —— 是真门；OOM 注入用 `malloc(SIZE_MAX)`，C 标准保证必失败。**但 `:1-6` 声称修复点是 `read_leaf_block` 两调用点的非 0 传播，而测试只验 `byte_unshuffle` 返回 −1，从未调用 `read_leaf_block`** ⇒ 调用点退回静默的回归不会被抓 | **须修**（声称的回归面未覆盖） |
| 35 | `validation/release02/phot_verify/wcs_lib.py` (93) 🔒 | `Wcs` 类、`_sip`、前向/逆向 | **`:3` docstring 声称「exact port of ACSD pc::WcsTransform」是伪引**。我逐行对拍生产：`:46-47` 用 Gauss–Seidel **顺序更新**（先改 dx，dy 用已被改写的 dx），生产 `wcs_transform.cpp:194-197` 是 Jacobi **同步更新**（f、g 都在原 (dx,dy) 上求值后一起加）。另 `:30-31` 的 `has_ap/has_bp` 要求「非全零」，生产 `:222` 只看指针非空 | **阻断**（伪引 + 可构造反例，见 §5-C） |
| 36 | `validation/release02/q3_additive_truth/src/step5_pairdetail.py` (153) 🔒 | 配对分析、`star_spatial`、`analyse_pair`、10 组 cases | `:4` `sys.path.insert(0, 'run/RELEASE-02/q3-additive-truth/src')` 指向 **gitignored 且不存在**的目录；`:7-8` `os.makedirs(OUT)` 在**模块级、`__main__` 之外** ⇒ 任何 import 本文件的脚本都会建目录（写未声明路径，违 AGENTS.md §6）；`:37/55-56/77-78/93/99-100` 共 8 处硬编码 2048/4096；`:50-52` `spatial_p2p_pct` 只在 5 个固定点取极值，**不是真极值**（是下界却按极值命名）；`:143-144` 匹配失败 `print('skip'); continue` ⇒ 脚本仍 exit 0 | **须修** |
| 37 | `validation/release02/q2_snr_smoothness/pollution_q2.py` (113) 🔒 | `cols`/`estimate`、泄漏段、200 次 MC、全权重段 | **`estimate()` 是代数恒等式**（我独立推导，§5-D）：`:40` 同一个设计阵 `A` 对两帧、`:41 Wd=(wH+wL)` 为常数 ⇒ `:42-44` 正规方程解出的 `coef` 就是两次独立拟合的**权重平均**，别无联合信息 ⇒ `g_art−g_cln ≡ frac × (ART 在基上的投影)`。`:60 predicted = frac*20.0` 用 `ART.max()`（**单点峰值**），`:56 measured` 用 `np.median(...[ov])`（**重叠区中位数**）⇒ 两个不同统计量并排印出，`:64-66` 既不比较也不 assert。**全文无一个 `assert`**。`:91 G` 定义后从不使用；`:108-109` 排序检查结构恒绿（`param_term` ~1e-15 vs `sigma_norm²`=1/100，H>L 由构造保证） | **阻断**（恒真门）+ 须修 3 条 |
| 38 | `validation/release02/q2_snr_smoothness/realdata/stageB_loci.py` (90) 🔒 | `crossing_scan`、`robust_fit`、三条 locus | `:4 ROOT = '/workspace/Astro CS Database'` **硬编码绝对路径**；`:6 np.load` 在模块级（缺失即 import 期死）；`:46 resid_max` 取自 **2.5σ 剪枝后的子集**（`:41 xs, ys = xs[keep], ys[keep]`）⇒ 按构造 ≤2.5σ_final，**结构上无法表达扫描真正见过的最差离群**（必查项 7「筛掉真信号」），且剪掉几个未报；`:53` 用 `thresh=7.5` 而 `:64/:76` 用 `8.5` ⇒ **同一水平带的两条边用不同半高**去定，量的不是同一条等值线；`:30-31` 返回 `None` 而 `:58` 无条件解引用 ⇒ 退化 locus 得崩溃而非「该 locus 不存在」 | **须修** |
| 39 | `validation/release02/q2_snr_smoothness/debug_gs.py` (41) 🔒 | 4 条重叠 strip、order-2 基、8 次定点迭代 | **结构对称型恒真门**（我独立推导，§5-E）：`:9 S` 与 `:12 BC` 全部同在 `cols()`（`:5-8`）给的 6 个基函数 `{1,U,V,U²,UV,V²}` 内，`:16 Z=S+B` 也在该空间内 ⇒ `:19-21` 的 6 基加权最小二乘在**任何子集**上残差恒等于 0 ⇒ `:37 g[n]=B[n]−c[n] ≡ 0` ⇒ `:38 R≡0`、`:39 spread≡0`，与 `:25` 的 8 次迭代和 `:17` 的 Wn 权重全部无关。**该脚本结构上不可能失败**。另 `:9` 给出闭式真值 S 却从不与 R/c 比对；`:25` 固定 8 轮、无收敛判据 | **阻断**（恒真门） |
| 40 | `validation/release02/c_delta_composition/delta_seam_cause.py` (59) 🔒 | 逐控制点 delta 离散度、LOO 步长、rms 对比 | `:3-6` 三个输入文件全不存在（`run/RELEASE-02/` 已被 gitignore）⇒ import 期即崩；`:49` 把恒真陈述当证据打印：`if only C is applied, corrected = M for every frame (spread=0 by construction)` —— 这是模型已含 C 的**定义后果**，与实现是否正确无关；`:51` `rms(C)` 在整块 (nF,K) 上算，而 `:10-12` 的 C 对不覆盖该控制点的帧**恒为 0** ⇒ 分母被结构性零撑大，压低「delta 占 C 的百分比」；`:28/:31` 静默丢 control 而 `:44` 只报存活数 | **须修** |
| 41 | `validation/release02/c_delta_composition/which_correction.py` (50) 🔒 | 4 种修正方案对照 + `spread()` | 同一输入数据链（`:4` 三文件全缺）；`:47-50` 两列统计并排印出但**只报 median 不报 n**，而 `spread()`（`:32-33`）实际筛掉 `s.size<2` 的控制点与 `Marr[k]<=1e11` 的控制点 ⇒ 表里的 n 与分母不是同一个数；`:9` 按位置索引与 `:20` 按名字索引**两套帧编号并存**；`:26` 注释称 weighted mean 而代码是无权 bincount | **须修** |
| 42 | `integration/p2_integrate/EVIDENCE.md` (94) 🔒 | §1–§7 全部表格 | **密集伪引，我逐条实测**（见 §5-F）：`p2_routing` 不在任何 `add_test`；`lib/phase2_int/` 整目录已不存在；`工程控制/` 顶层目录不存在；`:74` 声称 Oracle 复算「psfsw conventional coadd 与 C_out」而 `p2_oracle.py:8-9` **明写已退役不复算**；`:86-87` 称未注册进根 CMakeLists 而根 `:1509` 早已注册 | **阻断**（伪引） |
| 43 | `backend/phase2_fixture_main.cpp` (318) ⛔ | 仅子代理取证 | 子代理称是 fixture 生成器不是判据（无任何 CHECK，不存在「平行实现」），但 `declare_units_for_all` 四处静默失败后 `:316` 无条件打 `HIPS_FIXTURES_OK` return 0 | **建议**（我未复核） |
| 44 | `cli/test_unified_blocks_templates.py` (292) ⛔ | 仅子代理取证 | 子代理称 `:52` 引 `docs/engineering/CONFIG_CONTRACT.md:81` 含 `output_mode` **为裸行号伪引**（该行属 normalize，不涉 export）；`:259-261` normalize 的负例是一份仓内不存在的发明文档（用 `inputs[].light`，而真实退役形态用 `inputs[].product`） | **须修**（我未复核） |
| 45 | `integration/p2_integrate/p2_pixel_weight_test.cpp` (263) ⛔ | 仅子代理取证 | 子代理称独立 oracle 写得很好（`:44` 明示不调生产），但 `:148-149` 用**整数**判定控制节点（控制节点在 x=3.5+8i 半整数上）⇒ 8 个真实控制节点全被 `continue` 跳过，唯一入选的 `(31,31)` **在 4×4 网格之外**是外推点，而 `:154` 断言串却写 "at control nodes"（文案与行为不符）；`:18` 头部声明的判据 (D) 段在正文中**不存在** | **须修**（我未复核） |
| 46 | `testkit/testkit.spec.md` (139) 🔒 | §1–§9 全文 | 标 `ACTIVE_NORMATIVE`（`:5`）却 6 处点名 `eng/tools/testkit/check_testkit.py` 为规范性机器检查器（`:20/:43/:86/:101/:116/:122`）——我实测 `eng/tools/testkit/` **只剩 README.md**，`check_testkit.py` 不存在 ⇒ §4(A1 选择)/§5(A2 期望来源禁令)/§6(A3 故障注入)/§7(exit 0=TESTKIT_PASS) **全部无实现**。另 `registry.json` 与 `examples/registry.example.json` **逐字节相同** | **阻断**（规范标的工具不存在） |
| 47 | `testkit/examples/fixture_hash.py` (28) 🔒 | `PRE_FROZEN` + `main` | `PRE_FROZEN` 确为 `sha256("test")`（我核 `demo_fixture.txt` 内容为 `test`，4 字节）；`:16-18` 缺 fixture 时 **fail-closed** 返回 1 | **通过** |
| 48 | `testkit/fixtures/demo_fixture.txt` (0 行/4 字节) 🔒 | `od -c` = `t e s t` | 内容正确。**权威版记 0 行但非空文件** | **通过** |
| 49 | `api/__init__.py`, `arch/__init__.py`, `cli/__init__.py`, `pipeline/__init__.py`, `realdata/__init__.py`, `runtime/__init__.py`, `validation/release02/__init__.py`, `version/__init__.py` (各 0 行) 🔒 | 逐个 `wc -c` 核实 | 10 个 `__init__.py` 我逐个核实**确为 0 字节**。结构不一致（不是阻断）：`eng/tests/validation/` **本身无 `__init__.py`** 而 `release02/` 有，其下 9 个子目录又全无；`backend/`、`cli/` 含有大量非测试代码却配空 `__init__.py`；`runtime/` 只有 3 个 `test_*.py` 无实现代码（任务书的怀疑在该目录**不成立**） | **建议** |
| 50 | `unit/__init__.py`, `backend/__init__.py` (各 0 行) 🔒 | `wc -c` = 0 | 同上 | **建议** |

---

## 4. 发现清单

### 阻断（9 条）

| 编号 | 位置 | 结论 | 类别 |
|---|---|---|---|
| **BL-1** | `p2_seam_gate_test.cpp:50-59` + `:24` | 整个接缝数值门是与被测实现完全解耦的**代数恒等式**；`:24`「复用 P2-003」是伪引 | 恒真门(a) + 伪引 |
| **BL-2** | `debug_gs.py:9-16,19-21,37-39` | 真值与全部 B_n 同基 ⇒ 6 基 LS 残差 ≡ 0 ⇒ `spread ≡ 0`，脚本**结构上不可能失败** | 恒真门(b) 结构对称型 |
| **BL-3** | `test_common_abi.py:6,20` | `docs/api/` 整目录不存在 ⇒ `setUpClass` 抛异常 ⇒ **五个用例一次都没跑过**，而 `test_index.csv:2` 仍记 PASS/49/0 errored | 悬空引用 + 陈旧的绿 |
| **BL-4** | `test_common_abi.py:8-15,37-44` | 唯一的「头编译试金石」编译的是**测试自造的 9 行桩**，生产头 `module_api_v1.h` 从未进编译器 | 判据读的是桩 |
| **BL-5** | `rt005_registry_test.cpp:153` | `"acsd.phase1."` 12 字符 vs `substr(0,15)` 15 字符 ⇒ 恒 false ⇒ `:154` 死代码，文件头 `:6-7` 宣称的 P1-001 负向面从未被执行 | 恒假守卫 + 伪引 |
| **BL-6** | `rt007_artifact_store_test.cpp:3` vs `:90-96` | 文件头声称「篡改 path → 硬失败」，代码断言的是**篡改 path 后一切成功**；`:106-116` 四条「篡改」全由同一条 duplicate-id 规则满足，分辨力为零 | 伪引 + 判据重复验同一条规则 |
| **BL-7** | `test_p1004_joint_gate.py:180` | `assertEqual(res.get("run_id",""), "")` 是**反向门**——IMPL 修好就转红；`.get` 默认值使键缺失也绿 | 反向门 + fail-closed 伪装 |
| **BL-8** | `test_bench_candidates.py:96` | `assertRegex(sha256(data).hexdigest(), r"^[0-9a-f]{64}$")` 测的是 hashlib 性质，**永不可能红** | 恒真门(a) |
| **BL-9** | `wcs_lib.py:3` | docstring 声称「exact port of pc::WcsTransform」**为伪引**：前向 SIP 是 Gauss–Seidel 顺序更新，生产 `wcs_transform.cpp:194-197` 是 Jacobi 同步更新 | 伪引（可构造反例） |

> 子代理另报 3 条阻断，我**未独立复核**、按线索采纳待前台定夺：`anchors.json` 零消费者（`p2_upm_ma_test.cpp:11` 声称的 `upm_ma_oracle.py -> anchors.json` 数据流不成立，实际把锚值内联在 `:301-302`）；`anchors.json:321` `"kappa": Infinity` 非 RFC 8259 合法 JSON；`testkit.spec.md` 6 处点名的 `check_testkit.py` 不存在（**此项我已亲自复核成立**）；`EVIDENCE.md` 伪引群（**此项我已亲自逐条复核成立**，见 §5-F）；`echo_host_callback_test.c` 4 处 `ACSD_ECHO_ECODE_*` 编译错误（**我已亲自复核成立**）。

### 须修（18 条）

1. `field_constraints_oracle.py:423` — `all(k in ds or True ...)` **恒真子句**，O10 退化为长度检查（恒真门 a）。
2. `field_constraints_oracle.py:442` — 用模块级 `_REPO` 而非 `self.root`，`:12` 承诺的 overrides 遮蔽不了**唯一读 `lib/` 的那道门**。
3. `p2_sky_test.cpp:404,420` — 接缝门量的是模型在**自身训练样本**上的残差；独立真值 `field_true` 造好却全文弃用（往返自证）。
4. `p2_sky_test.cpp:234-238,241` — `bias()` 在 eval 失败时 `return 0.0`，使 `bw < 0.5*bu` 变恒真 ⇒ 被测模型 eval 失败会伪装成「加权压制成功」。
5. `p2_sky_test.cpp:305-316,341` — `rss_kb()` 读不到 `/proc/self/statm` 时 `return 0` ⇒ 内存门恒真；`:342` 的 `(void)r0;(void)r1;` 是作者自己不信这个量的残留。
6. `p2_sky_test.cpp:372-384` — 固定路径 `sky_plane_roundtrip_test.json`，先 save 后 open 读自己写的文件且**未先删** ⇒ **自愈判据**（第一跑红、第二跑转绿而缺陷仍在）。
7. `test_secure_loader.py:456-459` — L3 结构性恒绿：探针失败分支只打印数字码，从不打印 loader 消息串，而消息串是路径/sha 唯一泄漏面。
8. `test_secure_loader.py:4` / `test_hips_output_contract.py:4` — 伪引 `tasks/02_…` 与 `tasks/03_…`；**`tasks/` 目录全仓不存在**（我实测 `find -type d -name tasks` 零命中）。
9. `test_hips_output_contract.py:330,335` — 往返自证：`recompute_tree_hash` 实现就是 `tree_hash(entries)`，两条断言是同一方程 `f(x)==f(x)` 的两种写法。
10. `test_hips_output_contract.py:112-113` — 注释声称「fitsverify 在 stage 临时文件上执行」，但 `SpyStoreIO` 只有 5 种事件、**无 fitsverify 事件**，断言只是 `assertIn("signal", published_products())`。
11. `test_hips_output_contract.py:359` — 交叉 oracle 依赖的 `lib/infrastructure/aio/io/libfits_core_test.so` **未被 git 跟踪**（`git ls-files --error-unmatch` 报未匹配）⇒ 归档可被替换而仓库无记录（代码改了、归档没重跑）。
12. `oracle_rej.py:69` — `noise_flags` 赋值后**全文再无读取**（我 grep 确认仅 `:69` 一处赋值点）⇒ 生产侧噪声标志缺陷结构上不可能翻红；三个 fixture 的 flags 全是 3（恰好满足掩码）。
13. `p1wcs_oracle.hpp:290` — 引 `ipv_wcs.cpp:463-464`，实测该处是 `uv_x/uv_y` 计算；真值在 **`:512-513`**。且同一失效引用已扩散到 `ipv_solver.h:72`。
14. `core_scheduler_test.cpp:187` — 注释称「至多 1 个在途」，代码 `<= 2`；`p = now/70` ⇒ 2 节点在途即绿 ⇒ **上界错位一档**；且 `no_busy_spin` 名下无一测量自旋。
15. `rt002_budget_test.cpp:39,98` — 无 `CHECK(lease.acquired())` ⇒ acquire 一律失败时两个用例**恒绿**；`:51` 恒等三元 `i == 0 ? 4 : 4`。
16. `io_ownership_test.cpp:2,133` — 引 `eng/tools/check_aio_ownership.py`，**已从活树删除，只剩 `.pyc`**（我实测）；`:127 CHECK(rc != 0)` 无法区分 exit 1 / 127 / 信号打死。
17. `mutate_and_check.py:98-102` vs `:131-137` — configure 失败计 missed、**build 失败计 CAUGHT**，同一类失败两种相反计分。
18. `gate_assertions.py:102-108` — G5 反向极性（要求全部 provenance 为 `unverified`，正确核实反而转红 = 惩罚改进）；`:106` 的「无零点列」用 `re.compile("zero", IGNORECASE)` 找英文单词，`"wavelength_nm":0.0` 完全不被发现。
19. `runtime_event_stream_test.cpp:50-61` — `_WIN32` 分支把头部 `:15-20` 声明的 B/C/D/E 四组判据整体换成 3 条健康态必真检查后 `return`，注释却自称「（不静默、不假装绿灯）」⇒ **Windows 构建上是一个被文档化的假绿灯**（我已亲自复核）。

### 建议（12 条）

`p1_drz_test.cpp:619-622`（rows 为空时闭合门零执行）、`:345`（mutant 是两种归一化的杂合体，注释声称的「偏差 1/pf²」实为 ≈1/4.5）、`p1_drz_test.cpp:627-632`（死代码）；`p1wcs_oracle.hpp:268,306`（60 次定点迭代无收敛判据，发散时静默返回垃圾期望）、`:127,141`（收 `int n` 却 `(void)n` 无 `n>=3` 校验却无条件读 `u_xy[1]/u_xy[2]`）；`rt005_registry_test.cpp:5,6,36-52`（无 JSON parser、validate_config 从未被调用、12/22 factory 从未被调用）；`rt007_artifact_store_test.cpp:140-152`（roundtrip 只比 5/20 字段）；`p1_hips/adapter_entry_impl.cpp:4`（先例路径已迁）；`unit/p2_rej/CMakeLists.txt:5,11-12`（过期注释 + 双斜杠 + 缺 `if(TARGET)` 守卫）；`aio/CMakeLists.txt:41`（硬编码 python3）；`cli_fixture.py:139`（无源码指纹的进程级缓存）；`cli_test_hygiene.py:5,32`（`eng/ci/` 不存在、行号漂移）；10 个空 `__init__.py` 的收包结构不一致（`validation/` 本身无 `__init__.py` 而 `release02/` 有）。

---

## 5. 我主动构造的反例

### R-A · 推翻 BL-6（rt007「篡改检测」）—— **推翻成功**
- **构造**：把 `artifact.cpp::validate()` 中 unit / producer 一致性校验（若将来加上）整个删掉。
- **期望推翻**：`test_tamper_detection` 的 `:106/:110/:113/:116` 四条断言会转红。
- **结果**：**未能推翻**。四条全部由 `artifact_store.cpp:119-123` 的**同一条 duplicate-id 规则**满足（重新构造同 id 但不同内容 → 唯一 producer 拒绝）。把 unit/schema/producer 校验全删，四条照样全绿。**证明这四条分辨力为零**。
- 同型：`:90-96` 篡改 path 后断言的恰恰是 `.ok()`。

### R-B · 推翻 BL-9（wcs_lib「exact port」）—— **推翻成功**（我自己逐行对拍生产）
- **构造**：SIP order=1，`a_10 = 0.1`、`b_10 = 0.2`，其余系数 0；取某像素使 `dx₀ = 100`、`dy₀ = 5`。
- **期望推翻**：「exact port」成立 ⇒ 两者逐位相同。
- **结果**：**未能推翻**。
  - 生产 `wcs_transform.cpp:194-197`：`f = evalSip(A, 100, 5) = 0.1×100 = 10`；`g = evalSip(B, 100, 5) = 0.2×100 = 20`；**然后** `dx = 110; dy = 25`。
  - `wcs_lib.py:46-47`：`dx = 100 + f(100,5) = 110`；接着 `dy = 5 + g(**110**, 5) = 5 + 22 = 27`。
  - **差 2 px，且随 dx 线性放大**。
- **第二条分歧**：`wcs_lib.py:30-31` 的 `has_ap/has_bp` 要求「存在任一非零系数」，生产 `wcs_transform.cpp:222` 只看 `m_sip_ap != nullptr && m_sip_bp != nullptr`。当 AP/BP 是**全零数组**时，生产走直接支（施加零修正 ⇒ 逆变换**完全没有 SIP 改正**），`wcs_lib` 走 3 次迭代支（`:85-92`，给出正确逆解）⇒ 两者差**整个畸变量**，不是 epsilon。
- **影响链**：`wcs_lib` 是 `pair_ratio.py` 两帧源匹配的量具，量具本身不等价于生产 ⇒ phot_verify 的测光结论带系统性位置偏差。

### R-C · 推翻 BL-1（p2_seam_gate 接缝门）—— **推翻成功**
- **构造**：把生产接缝实现改错（令它返回 `C_B = -7.9` 而不是 −8.0）。
- **期望推翻**：该测试转红 ⇒ 它在验证生产接缝逻辑。
- **结果**：**未能推翻**。`:57` 的 `after` 完全由 `:54` 手填的常数决定，**生产代码根本不参与**；`overlap_med`（`:25-39`）是文件内 `static` 函数。改实现到任何值，本文件仍全绿。**该门与被测实现解耦。**
- 资源门半边同型：`:71` 把被测量设成阈值×1.2，生产 `:365` 判 `< thr` ⇒ 恒假 ⇒ `:74-75` 恒过。

### R-D · 推翻 BL-2（debug_gs）—— **推翻成功**（代数证明，非实验）
- **构造**：`S`（`:9`）与全部 `B_n`（`:12-15`）同在 `cols()` 的 6 基内，`Z = S + B`（`:16`）也在该空间内；`fit()`（`:19-21`）在 `tm` 上做 6 基加权 LS。
- **期望推翻**：若 `spread ≠ 0` 则该脚本有判别力。
- **结果**：对同基内数据，LS 残差**恒等于 0** ⇒ `coef` 精确复现 `B[n]` ⇒ `:37 g[n] ≡ 0` ⇒ `:38 R ≡ 0`、`:39 spread ≡ 0`。与 `:25` 的 8 次迭代、`:17` 的 Wn 权重**全部无关**。**该脚本输出的收敛曲线不能作为算法正确性的任何证据。**

### R-E · 推翻「pollution_q2 有联合拟合信息」—— **推翻成功**（代数证明）
- **构造**：令 H 帧无 artifact。
- **期望推翻**：`:42-44` 的正规方程含 H、L 的联合信息，`g_art − g_cln` 应依赖 artifact 的**空间形状**。
- **结果**：**未能推翻**。`:40` 同一设计阵 `A`、`:41` 常数 `Wd = w_H+w_L` ⇒ `coef = (AᵀA)⁻¹[ w_H/(w_H+w_L)·Aᵀz_H + w_L/(w_H+w_L)·Aᵀz_L ]`，即**两次独立拟合的权重平均**。故 `g_art − g_cln ≡ frac × (ART 在 6 基上的投影)`，与 artifact 形状**无关** ⇒ `:56 measured` 与 `:60 predicted` 都只是 `frac` 的倍数（且前者是重叠区中位数、后者是 `ART.max()` 单点峰值，**两个不同统计量**）。该实验只能测出 `frac` 本身。

### R-F · 推翻 EVIDENCE.md 的全部关键声明 —— **逐条推翻成功**（我亲自实测）
| EVIDENCE.md 声明 | 实测 | 结论 |
|---|---|---|
| `:3` 任务卡 `工程控制/…/tasks/P2-INTEGRATE-001.md` | `ls -d 工程控制` → No such file | ❌ 悬空 |
| `:5,12,17,80` 写域/构建源 `lib/phase2_int/` | `ls -d lib/phase2_int` → No such file | ❌ 悬空 |
| `:29` ctest `p2_routing` PASS | `grep -rn p2_routing --include=CMakeLists.txt` → 零命中 | ❌ 该测试不存在 |
| `:74` Oracle 复算「psfsw conventional coadd 与 `C_out`」 | `p2_oracle.py:8-9` 明写「psfsw_robust 已退役…**不再复算**其 conventional coadd / C_out」 | ❌ **被 oracle 自己的注释否定** |
| `:86-87` 未注册进根 CMakeLists | 根 `CMakeLists.txt:1509` `add_subdirectory(eng/tests/integration/p2_integrate p2_integration)` | ❌ 已被关闭却仍列为未决 |
| `:34` 「5/5 PASS, 0 skipped, 102 checks」 | 现行 CMakeLists 注册 8 个测试，含 3 个全文从未提及 | ❌ 落后一整代 |

### R-G · 推翻 BL-8（sha256 恒真门）—— **推翻成功**
- **构造**：把 samples 内容换成任意字节（含空、二进制、损坏）。
- **期望推翻**：`:95-96` 会红。
- **结果**：**未能推翻**。`hashlib.sha256(data).hexdigest()` 对任意输入必然是 64 位小写十六进制。

### R-H · 推翻 BL-7（p1004 联合门）—— **推翻成功（反向）**
- **构造**：给 `recorder.write_all` 传入真实 `run_id`。
- **期望推翻**：该门转红 ⇒ 它在验证互引语义。
- **结果**：**转红了**。`:180` 断言的正是 `run_id == ""` ⇒ **一个正确的修复被判为失败**。

---

## 6. 盲复算

**做法**：我先完成 58 份材料的独立通读与全部发现定稿（第 1–5 节在**未打开** `逐份判定-权威版.csv` 的状态下写定），再回查既有判定逐份比对。

**既有判定的实际内容**（`审核包-R2/分片清单/逐份判定-权威版.csv`）：
- 命中本片 **58/58** 份；
- `tier` 全部为 `HUMAN`；
- `reason` 只有 **2 种取值**：`默认保留（非产出面或非数据形态）` × **56**，`P3 手写面豁免: (^|/)fixtures(/|$)` × **2**。

**复算结论：偏严（且既有判定基本无内容）**。

- 既有判定对本片 **58/58 份**给出的是**同一句模板**（56 份「默认保留（非产出面或非数据形态）」+ 2 份 fixtures 豁免），**逐份内容层面的评估为零**。
- 我在本片中判出 **阻断 9 / 须修 18 / 建议 12**，其中**至少 9 个文件**（`p2_seam_gate_test.cpp`、`debug_gs.py`、`test_common_abi.py`、`rt005_registry_test.cpp`、`rt007_artifact_store_test.cpp`、`test_p1004_joint_gate.py`、`test_bench_candidates.py`、`wcs_lib.py`、`EVIDENCE.md`）此前被归为「默认保留」。
- 方向判定：**我的判定显著偏严**；没有发现任何一份文件被我判得过严到与既有判定冲突（既有判定对这些文件没有实质主张，故不存在冲突面）。
- 唯一需要为既有判定**让一步**的地方：`demo_fixture.txt` 与 `mosaic_legacy_contract.phase_config.json` 被归入「非产出面/手写面豁免」——就本片而言这两份**确实无缺陷**，豁免合理；但豁免理由是**形态**（不是内容），不能外推到本片其余 56 份。

---

## 7. 子代理派发记录

**派了 8 个**（要求 3–5，实际 8，因成员文件多车道重叠风险大而按覆盖面切细，其中 A/C 各配一名独立第二遍用于交叉裁决，另配 1 名正交扫描车道以抓逐行阅读会漏的系统性缺陷）。全部只读、无 git 写、未编译、未跑任何测试/实验脚本、未读 `/tmp/acsd_g08/`。

| 车道 | 覆盖 | 状态 |
|---|---|---|
| A1 | 5 份（field_constraints_oracle / p1wcs_oracle / oracle_rej / anchors.json / p1_drz_test）3000 行 | ✅ 全读完 |
| A2 | 同 A1（独立第二遍，用于交叉裁决） | ✅ 全读完 |
| B | 9 份（contracts/abi/api/version/config/cli）1708 行 | ✅ 全读完 |
| C1 | 9 份（validation/release02 + EVIDENCE + mutations）844 行 | ✅ 全读完 |
| C2 | 同 C1（独立第二遍） | ✅ 全读完 |
| D | 13 份（scheduler/runtime/budget/aio/backend-gate）1878 行 | ✅ 全读完 |
| E | 22 份（echo/cpu/conformance/CPU/feature/testkit/__init__）2995 行 | ✅ 全读完 |
| G | 正交扫描：全 58 份的模式扫描（恒红/恒绿/悬空/阈值/退役），重点精读 25 处上下文 | ✅ 已交付 |

### 逐条复核与**否决/降级**记录（前台亲自核对原文后裁定）

| 子代理结论 | 我的裁定 | 理由 |
|---|---|---|
| A1「O31/O30 疑似自证，判为恒真门」 | **否决** | 读完 `:792-798` 确认 O31 是**正反对照**：`not_rejected` 要求 21 个已知违例必被拒，`pos_bad` 要求 5 个合法正例必被接受。是有活性控制的好设计。 |
| A1「oracle_rej 校准分箱筛掉最差箱 → 须修」 | **降级为不成立** | 我重算：`lo=b·n//10, hi=(b+1)·n//10` 是**等计数分箱**，n≥500 时每箱样本数相同 ⇒ 筛选要么全过要么全不过（n<500 时全不过 ⇒ `used==0 ⇒ INSUFFICIENT_SAMPLES` 判红）。「只吃掉最差那一箱」不可能发生。A2 亦独立给出同结论（`CLASSIFY_V1_PROFILE.md:93` 明文允许且要求登记 `bins_skipped_small`，`oracle_rej.py:222` 已登记）。**我最初也中招，读完全文后撤回。** |
| A1/C2「`oracle_expected.inc` 落后一整代」 | **部分否决** | 我核对生成器 `:329` 写 `由 eng/tests/...` 而档案内为 `由 tests/unit/...` ⇒ **来源不同源成立**（我已实测）。但 A2 手工复算 `normal_3` 全字段与 `.inc` 逐位吻合 ⇒ **数值未漂移**。我把该条从「归档陈旧」改写为「归档可被无痕刷新且无外部锚」。 |
| C1「wcs_lib 是 exact port 为伪引」 | **采纳并升级为阻断** | 我亲自读 `wcs_lib.py:46-47` 与 `wcs_transform.cpp:194-197` 两侧，独立构造 R-B 反例并成功推翻。C2 独立给出同类反例，**两车道互证**。 |
| E「p2_sky `main(argv[1])` 筛选失配即全绿」判为**阻断** | **降级为须修** | 我核实 `eng/tests/unit/p2_sky/CMakeLists.txt` 逐字注册了 12 个组名 + `all`（`build/linux-control/p2_sky/CTestTestfile.cmake` 可见 13 条 `add_test`）⇒ ctest 路径不可达该缺陷。但缺陷本身真实：与 `p1_drz_test.cpp:773-776` 的 `g_pass==0 → return 2` 零执行守卫相比，**p2_sky 没有等价守卫**。 |
| E「debug_gs 是结构对称型恒真门」 | **采纳为阻断** | 我独立做 R-D 代数证明，结论一致。 |
| E「p2_seam_gate 是代数恒等式恒真门」 | **采纳为阻断（本片头号发现）** | 我独立做 R-C 反例并成功推翻。 |
| E「echo 4 处 `ACSD_ECHO_ECODE_*` 编译不过」 | **采纳为阻断** | 我实测 `grep -rn "ACSD_ECHO_ECODE_" eng/ lib/` → **零命中**；`types.h:48-56` 定义的是 `ACS_ECHO_ECODE_*`。硬错误。 |
| E「testkit 规范标的 `check_testkit.py` 不存在」 | **采纳为阻断** | 我实测 `eng/tools/testkit/` 目录存在但**只剩 README.md**。另我加验：`registry.json` 与 `examples/registry.example.json` **逐字节相同**。 |
| D「rt005 `:153` 恒假守卫 ⇒ 该门当前应为红」 | **采纳 `:153` 为阻断**；**否决「当前应为红」的运行态结论** | 恒假性是静态确定的（12 字符 vs 15 字符），我确认。但「phase1 的 `inspect()` 必 fail 从而整测当前为红」是**跨 `lib/` 的静态推导**，我未跑测试，**不断言当前红/绿**；已列为待前台执行项。 |
| D「rt007 四条篡改断言分辨力为零」 | **采纳为阻断** | 我 R-A 独立构造反例成功。 |
| D「core_scheduler 上界错位一档」 | **采纳为须修** | 我自己读过 `:174/:187`，独立确认 `p = now/70` ⇒ 2 节点在途 `p=2` ⇒ `2<=2` 绿。 |
| D「test_bench `:96` sha256 恒真门」 | **采纳为阻断** | 我 R-G 独立确认。 |
| D「p1004 `:180` 反向门」 | **采纳为阻断** | 我独立读过 `:180`，R-H 确认。 |
| D「io_ownership 引已删的 check_aio_ownership.py」 | **采纳为须修** | 我实测 `ls eng/tools/check_aio_ownership.py` → No such file，`__pycache__` 里只剩 `.pyc`。 |
| B「`test_common_abi.py` 整套已死 + 头编译判据读的是桩」 | **采纳为阻断** | 我独立 `ls docs/api/` → No such file，`find docs -name COMMON_ABI_V1.md` → `docs/engineering/COMMON_ABI_V1.md`；又读 `abi003_loader_probe.c:95-102` 确认 B3。 |
| B「`test_hips_output_contract.py:330/335` 往返自证」 | **采纳为须修** | 我读过两行并核对 `hips_output_store.py:111-119`（`recompute_tree_hash` 就是 `return tree_hash(entries)`）。 |
| B「`gate_assertions.py` G5 反向极性」 | **采纳为须修** | 我读过 `:102-108`，独立确认 `startswith("unverified")` 的惩罚改进结构。 |
| D「executor_provider_race 无数据竞争断言、TSAN 缺席」 | **采纳为须修（降级自阻断）** | 车道自陈 `:62-64` 注释「STARTED 快照允许为空」——`if (api)` 守卫与空值放行确实削弱判别力，但测试仍能抓「任务丢失 / provider 值域错 / 快照数量不足」。不是完全恒绿，故不判阻断。 |
| D「`mutate_and_check.py` 有自废武功的豁免清单」 | **否决** | 车道自查已确认 12 条全执行、无 skip/xfail/白名单，`:124` 的 ANCHOR-MISS 计 missed 是正确的 fail-closed。我独立复核 `:118-141` 同意。 |
| C1「`pollution_q2.estimate()` 是代数恒等式」 | **采纳为阻断** | 我独立做 R-E 代数证明。 |
| C1/C2「`step5` 归档 7 对 vs 代码 10 对，缺的恰是全部跨 panel 对」 | **采纳为须修（未跑脚本）** | 我核对 `:126-137` 确为 10 组 cases、前 7 组与子代理描述一致。**但我没有运行脚本去 diff `pair_detail.json`**（禁止跑实验脚本），故「归档只有 7 个 key」这一条属**子代理取证、我未复核**。 |
| A2「`p2_sky` 的 4 个 include 全部解析到真实生产文件」 | **采纳** | 我读 `p2_sky_test.cpp:15-19`，路径与车道所述一致。 |
| A2「p1_drz_oracle 验的是算子代数不是几何」 | **采纳为说明** | 我读 `:625-647` 确认 `a_jp` 取自生产算子的 `rows[j].hits`（raw 面积），非 `op` 内部 `c` 字段。 |
| E「`fixture_hash.py` 的 `PRE_FROZEN` 确为 `sha256("test")`」 | **采纳为通过** | 我 `od -c` 核实 `demo_fixture.txt` = `t e s t`。 |
| 各车道「`refute` 清单」（A2 的 10 条、B 的 10 条、C2 的 8 条、D 的 7 条、E 的 8 条） | **逐条采纳** | 车道自纠的怀疑（假悬空路径、oracle 自证、门恒真、退役对象有活调用者等）与我独立核实结果一致，其中多条正是任务书点名的「退役对象仍有活调用者」反型。 |
| G（正交扫描车道）「`runtime_event_stream_test.cpp:50-61` 的 `_WIN32` 分支是假绿灯」 | **采纳为须修（我亲自复核 `:50-61`、`:14-21` 后成立）** | 这是**我自己通读该片时漏掉**的（G 车道在我未读清单内）。我打开文件核实：`:50-61` 确只跑 3 条健康态必真检查后返回，`:53` 注释自称「不假装绿灯」而网络效果相反。**这是我本遍最实质的一次「子代理补到我」的采纳。** |
| G（正交扫描车道）「`cpu005_route_decision_test.cpp:346` 查错了对象，恒真」 | **暂不采纳** | 该文件在我未读清单内，且 G 自陈 cpu005 是**按命中点定位精读**而非全文读。我未独立复核，故只作线索登记、不计入判定。 |
| G（正交扫描车道）「本片内无「注释说零消费者但实际有调用者」的例子」 | **采纳为一条独立结论** | 与我 §3 中「`mosaic_legacy_contract.phase_config.json` 确有活调用者、不属退役零消费者」一致。任务书点名的这一型在本片**未复现**——本片命中的是**反向型**（注释说「零消费者/已物理删除」的 `psfsw_robust_weight`，G 实测 `lib/` 下 0 命中，声称成立；不成立的是 `EVIDENCE.md:86-87` 的「未注册进根 CMakeLists」）。 |
| G「7 个 validation/release02 脚本输入全不存在，全部是死脚本」 | **采纳为须修（部分未复核）** | 我已亲自核实 `delta_seam_cause.py:3-6`、`which_correction.py:3-4`、`stageB_loci.py:5-6`、`step5_pairdetail.py:4,7` 四处的输入路径指向 gitignored 的 `run/RELEASE-02/`；`.gitignore:17 run/*` 我亦已实测。`pollution_q2.py` 本身不读外部文件（纯合成），**故「7 个全死」这一表述我修正为「6 个死 + 1 个（pollution_q2）输入自产但结论恒真」**。 |

**否决计数**：7 个车道共提报约 **90 条**发现/反例；我**否决 3 条**（A1 的 O30/O31 判恒真门、A1/A2 的 oracle_rej 分箱筛除、A1 的「.inc 数值已漂移」）、**降级 2 条**（E 的 p2_sky 筛选器、E 的 executor race）、**改写口径 1 条**（.inc 从「陈旧」改写为「可无痕刷新且无外部锚」）、**部分采纳 1 条**（step5 归档数，我未跑脚本故不背书该数字）。其余采纳并已归入 §3/§4。

---

## 8. 自证段（可复跑命令）

> 全部为**只读**命令。**不要**加 `--build`、不要跑 ctest/pytest、不要跑任何仓内实验脚本。

```bash
cd "/workspace/Astro CS Database"

# —— 0. 基线 ——
git rev-parse HEAD                                  # 期望 850a9edefd47434b9ab71bc907c3de1e0814b323

# —— 1. 片成员清单与行数（权威版 1436–1501 行）——
sed -n '1436,1501p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" | grep -c '^      - "'
# 期望 58

# —— 2. BL-5 rt005 :153 恒假守卫（12 字符 vs 15 字符）——
python3 -c "print(len('acsd.phase1.'), '!=', len('acsd.phase1.cal'))"   # 期望 12 != 15

# —— 3. BL-3 docs/api 整目录不存在 ——
ls -d docs/api 2>&1 | head -1                        # 期望 No such file or directory
find docs -name COMMON_ABI_V1.md                     # 期望 docs/engineering/COMMON_ABI_V1.md

# —— 4. BL-4/BL-5 echo 未定义标识符 ——
grep -rn "ACSD_ECHO_ECODE_" eng/ lib/ | wc -l        # 期望 0
sed -n '48,56p' eng/tests/conformance/echo/include/acsd/echo/types.h   # 期望 ACS_ECHO_ECODE_*

# —— 5. BL-9 wcs_lib 伪引：两侧代码并排 ——
sed -n '193,198p' lib/algorithms/photometry/cpp/src/wcs_transform.cpp  # Jacobi 同步
sed -n '45,48p'  eng/tests/validation/release02/phot_verify/wcs_lib.py  # Gauss-Seidel 顺序
sed -n '221,223p' lib/algorithms/photometry/cpp/src/wcs_transform.cpp  # 指针非空判定

# —— 6. BL-2 debug_gs 同基：6 个基函数 vs 真值项数 ——
grep -n "c=\[np.ones_like\|c+=\[u\*u" eng/tests/validation/release02/q2_snr_smoothness/debug_gs.py
grep -n "^S=\|^BC=" eng/tests/validation/release02/q2_snr_smoothness/debug_gs.py

# —— 7. BL-8 sha256 恒真门（纯库性质，不跑仓内脚本）——
python3 -c "import hashlib;print(hashlib.sha256(b'').hexdigest())"
# 与 test_bench_candidates.py:96 的正则 100% 匹配 ⇒ 该断言永不可能红

# —— 8. BL-9 附：anchors.json 非法 JSON + 零读者 ——
grep -n '"kappa": Infinity' eng/tests/unit/p2_upm/oracle/anchors.json        # :321
grep -rn "anchors\.json" eng/ lib/ docs/ | grep -v config_separation_anchors
grep -c "fopen\|ifstream\|fstream" eng/tests/unit/p2_upm/p2_upm_ma_test.cpp  # 期望 0

# —— 9. 伪引批量核实 ——
find . -maxdepth 2 -type d -name tasks -not -path "./.git/*" | wc -l          # 期望 0
ls -d lib/phase2_int 2>&1 | head -1                                          # 期望 No such file
ls -d 工程控制 2>&1 | head -1                                                  # 期望 No such file
grep -rn "p2_routing" --include=CMakeLists.txt . | grep -v "^./run/" | wc -l  # 期望 0
sed -n '8,9p' eng/tests/integration/p2_integrate/oracle/p2_oracle.py          # 期望「不再复算」

# —— 10. 悬空引用 / 已删工具 ——
ls eng/tools/check_aio_ownership.py 2>&1 | head -1                            # 期望 No such file
ls eng/tools/__pycache__/ | grep -i ownership                                 # 期望只剩 .pyc
ls eng/tools/testkit/                                                          # 期望只有 README.md
diff -q eng/tests/testkit/registry.json eng/tests/testkit/examples/registry.example.json  # 期望 IDENTICAL

# —— 11. 未受版本控制的归档产物（BL-7 相关）——
git ls-files --error-unmatch lib/infrastructure/aio/io/libfits_core_test.so 2>&1 | head -2  # 期望 未匹配
sed -n '8,15p' eng/tests/api/test_common_abi.py                                # 期望看到自造 HEADER 字面量
sed -n '5,12p'  run/GOVERN-08/审核包-R2/分片清单/逐份判定-权威版.csv

# —— 12. 门恒真子句 / 死输入 / 错位上界 / 恒绿逃逸口 ——
sed -n '421,424p' eng/tests/contracts/product_family/field_constraints_oracle.py   # or True
grep -n "noise_flags\|flags" eng/tests/unit/p2_rej/oracle_rej.py | head -5          # 69 唯一赋值点
sed -n '174,187p' eng/tests/unit/core_scheduler_test.cpp                            # p=now/70 vs <=2
sed -n '234,241p' eng/tests/unit/p2_sky/p2_sky_test.cpp                             # return 0.0
sed -n '305,316p;341,342p' eng/tests/unit/p2_sky/p2_sky_test.cpp                    # return 0 / (void)
grep -n "field_true" eng/tests/unit/p2_sky/p2_sky_test.cpp                          # 期望仅 404/420 两处
sed -n '71,75p' eng/tests/unit/p2_seam_gate_test.cpp                                # 阈值×1.2
sed -n '364,365p' lib/infrastructure/cli/resource_gate.h                            # avg_equivalent_cores < thr
sed -n '131,137p' eng/tests/unit/aio/mutations/mutate_and_check.py                 # build-fail → CAUGHT
sed -n '102,108p' eng/tests/config/gate_assertions.py                              # unverified / ZERO_RE

# —— 13. 归档与生成器不同源 ——
head -1 eng/tests/unit/p2_rej/oracle_expected.inc
grep -n '由 eng/tests/unit/p2_rej/oracle_rej.py' eng/tests/unit/p2_rej/oracle_rej.py

# —— 14. 悬空行号引用 ——
sed -n '463,464p;511,513p' lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp
sed -n '288,291p' eng/tests/unit/p1wcs/p1wcs_oracle.hpp
```

**待前台执行项（我未做，纪律禁止）**：
1. 跑 `rt005_registry` 确认 §2 第 3 条的运行态后果（当前红/绿）。
2. 确认 `echo_host_callback_test.c` 的 4 处改名后能否编译。
3. 跑 `eng/tests/version` 与 `eng/tests/api` 后回填 `eng/tests/test_index.csv`——该索引对本片记的绿是**陈旧的**（BL-3 已证 `test_common_abi.py` 整族未跑）。
4. 重跑 `oracle_rej.py --emit-inc` 并 diff，确认 `.inc` 数值是否真的未漂（我只证明了**可被无痕刷新**）。

---

*本遍全程只读：无 git 写、无仓内文件修改、无编译、无 ctest/pytest、无任何实验或测试脚本运行、未读 `/tmp/acsd_g08/`。全部结论来自亲读原文 + 手工代数推导 + 逐条实测的只读核实命令。*

> ⚠️ **工作区异常披露（诚实起见）**：交付前 `git status` 显示 `docs/` 下 4 个文件为已修改未提交状态（`ACSD_DESIGN.md`、`DOCUMENT_GOVERNANCE.md`、`FROZEN_GATE_INVENTORY.md`、`MODULE_MAP.md`，时间戳落在本会话时间窗内）。
> **本审稿车道未对任何仓内文件发起过写操作**——我本会话的唯一 `write` 调用就是本交付件（已确认 `git check-ignore` 显示 `run/*` 被 `.gitignore:17` 忽略，故本文件不进入版本控制面）。这些 `docs/` 改动来自**同一工作区内并发的其他 agent**，其内容（把 `实验/photometric-magnitude/results/gates.json` 换成报告正文正本、并写明「判据自身绿不构成该判红的证据」）与负责人本轮裁决同向。
> **登记为 UNRESOLVED，移交前台**：这些改动未提交、未由本车道产生、也不在本片 58 份成员的判定范围内，但它们会改变后续车道读到的文档基线，请前台确认其归属与是否入册。
