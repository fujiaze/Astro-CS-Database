# 审稿-P1-ALG-cosmetic-001 · G08-05 对抗审稿第 1 遍

- **片号**：`ALG-cosmetic-001`
- **层**：`lib/algorithms/cosmetic`（生产源码）
- **基线**：仓库 `/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- **片清单**：`.yaml:83-108`（权威版 A1），成员 18 份 / 4307 行
- **本人立场**：红队。默认现行结论为错；「检查通过」的机制不作为正确性证据。
- **纪律自证**：零 git 写、零编译、零二进制执行、未读 `/tmp/acsd_g08/`、未修改任何仓内文件（唯一写入为本交付件）。

---

## 1. 读完了吗

**覆盖率：18/18 份，4307/4307 行 = 100%**（分母为片清单声明的 `实际行数: 4307`，与 `wc -l` 实测逐份吻合）。

计数口径：分母取权威片清单 `片清单-权威版.yaml:85-87` 的 `成员份数: 18` / `实际行数: 4307`；分子按 read 工具返回的行号范围逐份累计，每份读至 `End of file`。

| # | 成员文件 | 声明行数 | 实读行数 | 谁读的 |
|---|---|---|---|---|
| 1 | `src/module_entry.cpp` | 1266 | 1266 | **本人**（1–450 / 451–870 / 871–1266 三段读完） |
| 2 | `tests/p1cos/p1cos_tests_badcol.cpp` | 694 | 694 | 子代理 #2 全读 + 本人抽读 95–140 |
| 3 | `tests/p1cos/p1cos_tests_core.cpp` | 545 | 545 | 子代理 #1、#2 全读 + 本人抽读 55–125 |
| 4 | `tests/p1cos/p1cos_fixtures.hpp` | 359 | 359 | 子代理 #1 全读 + 本人抽读 100–235 |
| 5 | `tests/p1cos/p1cos_oracle.hpp` | 248 | 248 | 子代理 #1 全读 + 本人抽读 54–78 |
| 6 | `README.md` | 196 | 196 | **本人** |
| 7 | `tests/p1cos/p1cos_tests_selfcheck.cpp` | 156 | 156 | **本人** |
| 8 | `integration/acsd_p1_cosmetic.integration.json` | 152 | 152 | **本人** |
| 9 | `tests/p1cos/p1cos_test_main.hpp` | 148 | 148 | **本人** + 子代理 #1 |
| 10 | `tests/p1cos/p1cos_tests_perf.cpp` | 117 | 117 | 子代理 #1、#2 |
| 11 | `CMakeLists.txt` | 112 | 112 | **本人** |
| 12 | `tests/p1cos/CMakeLists.txt` | 80 | 80 | **本人** |
| 13 | `include/acsd/cosmetic/types.h` | 74 | 74 | **本人** |
| 14 | `memory.md` | 71 | 71 | **本人** |
| 15 | `module.yaml` | 68 | 68 | **本人** |
| 16 | `tests/p1cos/p1cos_tests_main.cpp` | 12 | 12 | **本人** |
| 17 | `src/module_exports.map` | 6 | 6 | **本人** |
| 18 | `src/acsd_p1_cosmetic.def` | 3 | 3 | **本人** |
| | **合计** | **4307** | **4307** | |

**未读完的：无。** 本人独立完整读完 11 份（含全部 4 份文档/合同/声明面 + 全部 4 份构建/导出面 + 1266 行生产源码本体）；其余 7 份测试源码由子代理逐行读完，其结论由本人抽读原文复核（见 §7）。

---

## 2. 本片判定：**需修**

不判「通过」——本片存在 **4 条阻断**（运行期资源泄漏、fail-open 静默降级、状态锚悬空、注册词表双轨）。

### 最重 3 条

1. **【阻断-1】`module_entry.cpp:797` 在取得 executor 租约之后分配内存，OOM 路径下租约永不归还，且进程全局 OpenMP ICV 被永久改写。**
   `:765` 取租约 → `:769` 写全局 ICV → `:797 bytes out_plane(plane_bytes);`（vector 构造，可抛 `std::bad_alloc`）→ `:826-829` 才还租。异常逃逸到 `cos_execute` 的 `:1161`/`:1165` catch，**只重置 `inst->state`，不 release、不还原 ICV**。违反 `lib/include/acsd/core/executor.h:8` 的 `Σ(active) ≤ budget` 进程级不变量 ⇒ 每次 OOM 永久扣减预算，累积后全进程 cpu_heavy 模块一律 `ACS_ERR_BUDGET`；ICV 残留波及同进程 drizzle/noise_snr/hips_writer。**同函数内 `:779-793` 的取消宏已正确「先还租再返回」，证明这是遗漏而非设计。**

2. **【阻断-2】`module_entry.cpp:736` 把可选平面的「键存在但类型错」静默当成「键缺席」，fail-open 伪装成合同里的 null 语义。**
   合同 `integration.json:64` 只授权「键缺席 / JSON null → legacy NULL」。代码写的是 `if (mkv[bi].kind != 's' || !b64) continue;` —— 把 `'n'`（数字）、`'b'`（布尔）等**类型错误**一并吞成缺席。后果：`"master_dark": 12345`（应为致命参数错）⇒ 热像素检测被静默关闭，`ac_correct_frame` 收 NULL（`cosmetic_corrector.cpp:243` `if (dark && hot_sigma>0)`）⇒ 输出恒等帧、`hot=0`，**返回 `ACS_OK`**。**同一文件对必需平面是严格的**（`:719-722` `data_base64` 类型错 ⇒ `PARAM+110`），config 侧也严格（`:399-403` 类型错 ⇒ `PARAM+102`）——**同一份代码里两套宽严标准，正是伪装成 fail-closed 的 fail-open**。

3. **【须修-1】p1cos 全套 oracle 与生产公式逐行同源，且全部夹具 `mad≡0 ⇒ σ≡0`，检测判据最核心的数值零判别力。**
   `p1cos_oracle.hpp:54-78` 的 `stats_oracle/detect_hot_oracle/detect_cold_oracle` 与 `lib/algorithms/calibration/src/cosmetic_corrector.cpp:124-155` **逐行对应**（`med + hot_sigma*1.482602218505602*mad`、`dark[i] > thr`），只差 float→double。而 `fixtures.hpp:81/151/192/237/301/340` 六处暗场一律「常量 + ≤9 稀疏 spike」（本人实测 32×32=1024 像素、≤9 spike ⇒ >99% 像素等于中位数 ⇒ `MAD=0`）。**两个 mutant 全绿**：把 `1.482602218505602` 改成 `0.0`、或把 σ 恒置 0，测试无一转红。这正是负责人点名的**自洽式断言**：同一套定义式既当被检量又当期望量。

---

## 3. 逐文件清单

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `src/module_entry.cpp` 1–450 | 文件头冻结合同声明、base64 编解码、迷你 JSON 解析器、CFG 词表 | base64 严格解码 `:131-179` 实现正确（补齐后出现数据、孤立单字符组均拒）；`jc_parse_object` 重复键/unknown 键均拒 `:338-340`；`kv_u64` 溢出防护 `:427` 到位 | 通过 |
| 同上 451–870 | op 词表、`cfg_fill`、`decode_plane`、`exec_run` 主体、租约 | **`:736` fail-open**（阻断-2）；`:707` w*h 溢出护栏在 `plane_bytes` 计算之前，顺序正确；`:757-773` executor 缺失/acquire 失败均硬 `BUDGET`、**禁单线程降级**（合规）；`:779-793` 取消宏先还租后返回（**正确**，反衬 797 遗漏） | **需修** |
| 同上 871–1266 | describe/validate/plan/create/execute/inspect/cancel/destroy/query | **`:797` 租约泄漏**（阻断-1）；`describe` 空 module_id 放行有 `:862-865` 书面理由；`cos_plan` 输出 10 键与 `integration.json:104` 键表逐字吻合；`:1104-1106` execute 起点清空输出=事务性正确；`:1159` 失败后回 CREATED 可重复 | **需修** |
| `include/acsd/cosmetic/types.h` | 74 行全文 | 常量/词表齐全，但 **`:34-41` 的 8 个 `ACSD_COS_CFG_KEY_*`、`:62-68` 的 7 个 `ACSD_COS_MAN_KEY_*`、`:57-58` 的两个 method 宏，consumer 数 = 0**（实测 grep：`module_entry.cpp` 全用裸字符串字面量）。头文件自称「三方一致」的唯一事实源，实际是**不被使用的第二份真值** | 须修 |
| `README.md` | 196 行全文 | **`:6` 称 entrypoint「函数体零调用」⇒ NOT_IMPLEMENTED**，与 `module_entry.cpp:1237-1247` 完整九操作 vtable 直接矛盾；`:13`/`:28` 引 `CMakeLists.txt:321-333`，实测根库在 `:668-685`；`:25` 称「无版本化 query 入口」与 `:24`「在位」自相矛盾；`:3` 状态行与 `module.yaml:19` 的 `CONTRACT_READY`、产品清单 `acsd.product.json:15` 的 `IMPLEMENTED` 三方打架 | 须修 |
| `tests/p1cos/p1cos_tests_selfcheck.cpp` | 156 行全文 | **本片设计质量最高的一份**：`:38-80` guard-path 用三态（合法名/空串名/nullptr 名）**证明失败路径真的被执行过**——这正是对抗「恒真门没有证据资格」的正解；`:114-125` fork/execve 子进程注入，`:127-132` 注入后仍 PASS 判红 | 通过 |
| `integration/acsd_p1_cosmetic.integration.json` | 152 行全文 | `:129` 声称 `eng/tests/unit/cosmetic_integration_test.cpp` 存在——**核实为真**（未悬空）；`:104` plan 键表与 `cos_plan` 实际输出一致；`:131-144` failure_contract 11 条与代码逐条可对；`:64`/`:78` 把 master 端口的 null 语义写死为「键缺席/JSON null」，**代码未按此收窄 ⇒ 阻断-2 的合同侧证据** | 须修 |
| `tests/p1cos/p1cos_test_main.hpp` | 148 行全文 | `:63-65` `fault_name_usable` 已把恒真门换成运行期内容判定（**修复有效**）；**`:126` + `:138-141`：组名零匹配与全通过不可区分，均 `return 0` + 打印 PASS**；`:87-97` `P1COS_CHECK_EQ` 无 faultname 形参 ⇒ 科学含量最高的计数断言**不可注入** | **需修** |
| `CMakeLists.txt` 112 行 | 全文 | `:40-53` 4 adapter/production TU 全部存在；`:83` `AC_API=` 本地化 + `:104-111` version-script 双保险，导出面净化正确；`:38-39` 注释称「与根 acsd_calibration:333 逐文件一致」——**实测根库 `:668-677` 是 5 TU 且 333 行不存在**，该断言为假（无链接影响，因 `ac_api.cpp` 对 photometry 零引用） | 须修 |
| `tests/p1cos/CMakeLists.txt` 80 行 | 全文 | 4 executable / 8 `add_test` 对得上；**全部 `target_link_libraries(... acsd_calibration ...)`，无一处链接 `acsd_p1_cosmetic`** ⇒ 本片 1266 行 adapter 在自己的测试台里**零覆盖**；`:80 p1cos_badcol_selfcheck` 是全片唯一「注入必红」真机制所在 | 须修 |
| `module.yaml` 68 行 | 全文 | 55-62 `source_symbols` 7 个符号全部在 `calibration/src/` 真实存在（核实无悬空）；`:26-28` node_operations 与 `types.h:48-49` 一致；`:1`/`:13` 含日期与「冻结」历史叙事，**违反 AGENTS.md §5「文档无日期/版本流水」** | 建议 |
| `memory.md` 71 行 | 全文 | `:52-65` 源码核对结论逐条可验证且与生产一致（含 median 偶数双中位 `(hi+lo)*0.5f`、MAD 定义、IDW 4 方向）；`:34-71` 整段进度日志带日期/任务号/commit 叙事，**违反 §5** | 建议 |
| `tests/p1cos/p1cos_tests_main.cpp` 12 行 | 全文 | 纯转发 `:10-12`；`p1cos_run_core_groups` 在 `core.cpp:537` 有定义 | 通过 |
| `src/module_exports.map` 6 行 | 全文 | `global` 仅 `acsd_module_query_v1`，`local: *` | 通过 |
| `src/acsd_p1_cosmetic.def` 3 行 | 全文 | 与 .map **完全对齐**，两平台无分歧 | 通过 |
| `tests/p1cos/p1cos_tests_core.cpp` 545 行 | 子代理全读 + 本人抽读 55–125 | **自洽式**：计数与修复期望经 `correct_oracle`（同式转写）取得；`:81` `const_field_bitwise` 等注入点只翻 `cs.failures`、**不改被测行为** | 需修 |
| `tests/p1cos/p1cos_tests_badcol.cpp` 694 行 | 子代理全读 + 本人抽读 95–140 | **本片唯一正确的反恒真范式**：`:102` `Inject g_inj` 真注入缺陷、`:133-136` 非退化对照（「修复值必须与原坏值不同，否则判据恒真」）；`:8` 明写「缺对照的断言按恒真门处理，不计证据」 | 通过（作范式） |
| `tests/p1cos/p1cos_oracle.hpp` 248 行 | 子代理全读 + 本人抽读 54–78 | **与生产逐行同源**（见须修-1）；`:6` 注释自称「不复制同一实现」**与事实不符**；`:233-235` `close_enough` 在 `want=±Inf` 时 `fabs(inf-inf)=NaN` 恒红 | 需修 |
| `tests/p1cos/p1cos_fixtures.hpp` 359 行 | 子代理全读 + 本人抽读 100–235 | 六处暗场 `mad≡0`（须修-1）；`:54` 「spike = med+1990 ADU」**算术错误**（实为 +995）；`:344` 「2 的幂量化噪声(float 精确)」**不成立**（0.001 非 2 的幂）；`:18-21` 用 `std::numeric_limits` 却**缺 `#include <limits>`** | 需修 |
| `tests/p1cos/p1cos_tests_perf.cpp` 117 行 | 子代理读 | `:70,83,89` 用 float `==` 而非 core 的 `bit_equal`，口径不一致（NaN 假红、±0 误判通过） | 建议 |

---

## 4. 发现清单

### 阻断（5）

**【B-0（最强，本轮指定型）】`p1cos_tests_badcol.cpp:610` 与 `:635` —— 恒真门：非退化对照两侧实测均为空集**
- `:610` `check(wide_cut.nd == wide_ok.nd, "10g 丢弃宽标记不得影响已修（单列）判定")`
- `:635` `check(s_cut.ns == s_ok.ns, "10j 科学帧路径的已修（单列）判定不受宽标记守卫影响")`
- **本人复核成因**：fixture `dk2`（`:588-592`）的唯一缺陷是相邻列 12/13 各 −400（宽段），`sci3`（`:614-618`）是列 40-42 各 −500（宽段）。二者**都没有单列缺陷** ⇒ dark 路径无 `MASK_REPAIRED`、科学帧路径无单列修复 ⇒ `nd`/`ns` **结构上恒为 0**，比较是 `0 == 0`。
- **加重**：`apply_wide_budget`（`cosmetic_corrector.cpp:582`）只写 `MASK_WIDE` 位，**结构上不可能改动 REPAIRED 列计数** ⇒ 即使 fixture 非空，这两条仍恒真。
- 为什么是问题：断言消息把它们写成有效的「非退化对照」，排在 `:608`/`:633` 的真判别力断言之后，**读起来像已覆盖、实则零判别力**。恒真门违反 AGENTS.md §8「判据须在正确实现下绿、注入缺陷时红」的后半句。

**【B-1】executor 租约在 OOM 路径泄漏 + 全局 OpenMP ICV 永久污染**
`lib/algorithms/cosmetic/src/module_entry.cpp:797`
- 链路：`:765 ex->acquire()` → `:769 ac_set_num_threads()` → **`:797 bytes out_plane(plane_bytes);`（可抛 `bad_alloc`）** → `:826-829 ex->release()` + 还原 ICV。
- 逃逸：`cos_execute` 的 `:1161` / `:1165` catch **只重置 `state`**。
- 违反：`lib/include/acsd/core/executor.h:8,60` 的 `Σ(active) ≤ budget` 进程级不变量。
- 可达：`:707` 只把 `w*h` 限到 1e8 ⇒ f64 单平面 800 MB，叠加 `:715/:726` 已驻留的 light/pdark/pbias，峰值约 3.2 GB，OOM 现实。
- **同函数 `:779-793` 的取消宏已正确先还租** ⇒ 遗漏而非设计。

**【B-2】可选平面类型错静默降级为缺席（fail-open）**
`lib/algorithms/cosmetic/src/module_entry.cpp:736`
- `if (mkv[bi].kind != 's' || !b64) continue;` 把 `'n'`/`'b'` 类型错吞成缺席。
- 合同侧仅授权 null：`integration/acsd_p1_cosmetic.integration.json:64,78`。
- 后果：检测静默关闭、输出恒等帧、`hot=0`、**返回 `ACS_OK`**；宿主无法与「检测跑了但零坏点」区分。**加重**：输出 manifest（`:629-656`）的键集为 `schema_version/width/height/dtype/out_base64/op/hot/cold`，**无任何「母版是否到位」标志** ⇒「母版到位且真检出 0 缺陷」与「母版缺席纯恒等空转」产出**逐字节相同的 manifest**。而同模块的**兄弟路径坏列**在 `module_adapters.cpp:2866` 明文要求「母版不可得 ⇒ 该路径不参与，但**必须留痕**（不得静默退化）」——**规范对兄弟路径执行了，对坏点路径未执行**。
- 同文件对必需平面严格（`:719-722`）⇒ **两套宽严标准 = 伪装的 fail-closed**。

**【B-3】README 的机读状态锚点工具已被物理删除，只剩 `.pyc`（任务书点名的反模式）**
`lib/algorithms/cosmetic/README.md:4,6,8`
- README 把 `python3 eng/tools/quality/check_module_map.py` 当作现状复测的**唯一依据**，`:8` 明写「以本注记与**检查器输出**为准」。
- **本人复核**：`git ls-tree -r HEAD | grep -c check_module_map.py` → **0**；`git ls-files | grep check_module_map` → **不在版本控制**；磁盘上只剩未跟踪的 `eng/tools/quality/__pycache__/check_module_map.cpython-313.pyc`（82591 B）。
- 为什么是问题：**一个已不存在的工具的输出，被当作本模块的现行机读状态写进了文档**——这正是「读数被当生产上报」的变体。

**【B-4】全局注册声明被证伪 + 词表六项双轨**
`integration/acsd_p1_cosmetic.integration.json:146-150` 声称 `global_dag_registration.status = "delegated"`、本模块尚未全局登记。
- **本人复核**：`lib/infrastructure/pipeline/module_ports.registry.json:150-196` **已注册**，且与描述符**六项全不一致**：

| 维度 | registry（生产事实） | integration.json / module.yaml（文档声明） |
|---|---|---|
| module_id | `acsd.phase1.cosmetic` | `acsd.p1.cosmetic` |
| operation | `cosmetic_correct` | `correct_frame` / `correct_frame_f64` |
| entry | `acsd_phase1_cosmetic_v1` | `acsd_module_query_v1` |
| 端口 | `p1_calibrated` / `p1_cleaned` | `p1.calibrated` / `+master_dark` / `+master_bias` / `p1.cleaned` |
| carrier | `output_dir_file` | `inline_base64_manifest` |
| data_schema_id | `DATA-P1-COSMETIC` | `DATA-P1-COS` / `DATA-P1-CAL` |

- registry **完全没有 master_dark / master_bias 输入端口**，与 `integration.json:54-79` 宣称的两个可选输入面直接矛盾 ⇒ 「母版是否接线」在两套口径下会得到**相反答案**。
- 同时 README:6/:24 的 `NOT_IMPLEMENTED` 推理无效：插件 ABI 的唯一合法调用者本就是动态加载器（`lib/infrastructure/pipeline/orchestrator/cpp/src/dll_loader.cpp:525` `dlsym`），静态 grep 必然零命中；而进程内路径 `p1_op_cosmetic` 完全在线（`module_adapters.cpp:2821` 定义、`:13916` 分派，本人均已核实）。

### 须修（10）

| # | 位置 | 问题 |
|---|---|---|
| S-1 | `p1cos_oracle.hpp:54-78` + `fixtures.hpp:81,151,192,237,301,340` | oracle 与生产公式逐行同源；且六处暗场 `mad≡0 ⇒ σ≡0`，`1.482602218505602` 与整条 MAD 标度链**零判别力**（mutant 全绿）。**自洽式断言** |
| S-2 | `p1cos_test_main.hpp:126,138-141` | 组名零匹配与全通过**不可区分**，均 `return 0` + PASS。组名在 `tests/p1cos/CMakeLists.txt:41-44` 与 `core.cpp:539-542` 两处硬编码无交叉校验 ⇒ 一次改名即让 ctest 把 `p1cos_units` 报成 PASSED 而**实跑 0 条断言**。**恒真门** |
| S-3 | `p1cos_test_main.hpp:74-79` | 故障注入**不短路被测代码**，且 `:77-79` 规定注入后后续具名 CHECK 全部无条件失败、`cond` 不再求值 ⇒ 把 `:81` 改成 `P1COS_CHECK(cs, true, "const_field_bitwise")` 恒真断言，selfcheck 仍报「必败验证通过」。**自检无法区分有判别力的断言与恒真断言** |
| S-4 | `README.md:6,13,25,27,28` | 状态自相矛盾且整体过期：「零调用/NOT_IMPLEMENTED」与真实 vtable 矛盾；行号 `:321-333` 不存在；「不声明 IMPLEMENTED」与 `acsd.product.json:15` 的 `IMPLEMENTED` 冲突 |
| S-5 | `include/acsd/cosmetic/types.h:34-41,57-58,62-68` | 17 个词表宏 consumer = 0，adapter 全用裸字面量 ⇒ 头文件是**不被使用的第二份真值**，改任一处不触发另一处 |
| S-6 | `tests/p1cos/CMakeLists.txt`（全部 target） | 无一处链接 `acsd_p1_cosmetic` ⇒ 本片 1266 行 adapter 在自家测试台零覆盖 |
| S-7 | `CMakeLists.txt:38-39` | 「与根 acsd_calibration:333 逐文件一致」为假（根 `:668-677` 是 5 TU，333 行不存在）；虽无链接影响，但闭包合法性的书面论据失效 |
| S-8 | `p1cos_tests_badcol.cpp:583` | **代数恒等式冒充断言**：`check(a.n == a.ns + (a.n - a.ns) && a.n >= a.ns, ...)`。第一个合取项恒等于 `a.n == a.n`（本人用穷举验证：`a ∈ [-100,100]` × `ns ∈ [-20,20]` 共 8481 组，**反例 0 个**）。这是「用被检量自身造期望量」的自洽式断言，且断言消息宣称的语义只有后半句 |
| S-9 | `p1cos_tests_core.cpp:242` | `P1COS_CHECK(cs, r.n_hot <= raw_sum, "count_mask_relation")`：`:237` 已钉 `raw_sum=8`、`:241` 已钉 `r.n_hot=3` ⇒ `3 <= 8` 是两条常量的平凡比较，**零新信息** |
| S-10 | `README.md:13,28,171` + `memory.md:55,64` | 行号锚点系统性漂移：README 三处称 `CMakeLists.txt:321-333` = `acsd_calibration`（该子目录仅 91 行，根 `:321-333` 是 `acsd_platform_math` 注释块，真值根 `:668`）；README:90/memory.md:64 的 `ac_api.cpp:228-263` 实为 `ac_generate_master_flat_f64`（真值 `:277 ac_correct_frame_f64`）；memory.md:55 的 `:108-122` 实为 `ac_generate_master_dark`（真值 `:155`）。**注**：`cosmetic_corrector.cpp` 的 6 处锚点经核**全部精确命中** ⇒ 漂移高度集中在 `ac_api.cpp` 与头文件路径，非普遍腐化 |

### 建议（6）

| # | 位置 | 问题 |
|---|---|---|
| G-1 | `module_entry.cpp:602,618` | `decode_plane` 的 `what` 形参被 `(void)what;` 丢弃，三条错误串字面完全相同 ⇒ 运维无法判断坏的是哪一路平面 |
| G-2 | `p1cos_fixtures.hpp:54,344` | 「spike = med+1990 ADU」算术错误（实为 +995）；「2 的幂量化噪声(float 精确)」不成立（0.001 非 2 的幂）。无依据经验值 |
| G-3 | `p1cos_fixtures.hpp:18-21` | 用 `std::numeric_limits` 却缺 `#include <limits>`，靠传递包含侥幸编译 |
| G-4 | `p1cos_oracle.hpp:233-235` | `close_enough` 在 `want=±Inf` 时 `fabs(inf-inf)=NaN` ⇒ 恒红（latent） |
| G-5 | `module.yaml:1,13`、`memory.md:34-71` | 含日期/任务号/commit 的历史流水叙事，违反 AGENTS.md §5「文档无日期/版本/commit」 |
| G-6 | `module_entry.cpp:661` | 注释「状态护栏与租约在调用方」**与代码相反**：`cos_execute` 不取租约，租约在 `exec_run:752-829` |

---

## 5. 我主动构造的反例

### R1（推翻 S-1：`1.482602218505602` 零判别力）—— **推翻成立**
构造：取 `FIX-COS-B` 实际尺寸 32×32 = 1024 像素，暗场为常量 5.0 + ≤9 个 spike（本人读 `fixtures.hpp:100-126` 与 `:81` 实测构造）。`|src−med| = 0` 的像素 ≥ 1015/1024 ⇒ **MAD = 0 ⇒ σ = 0**。
期望推翻：把 `1.482602218505602` 改成 `0.0`（或把 σ 恒置 0）应使测试转红。
结果：阈值退化为 `med`，检测退化成 `dark[i] > med`；因 spike=1000 ≫ 5，**判定结果逐位不变** ⇒ mutant 全绿。**推翻成立**。

### R2（推翻 B-2：fail-open 可达）—— **推翻成立**
构造：manifest 写 `"schema_version":1,"width":32,"height":32,"dtype":"f32","data_base64":"...","master_dark":12345`（合法 JSON 数字，非字符串）。
- `:732 kv_find` 命中 `master_dark`；
- `:736 mkv[bi].kind != 's'` 为真 ⇒ `continue` ⇒ `pdark` 保持空；
- `:806 pdark.empty() ? NULL : ...` ⇒ legacy 收 NULL；
- `cosmetic_corrector.cpp:243 if (dark && hot_sigma > 0.0f)` 为假 ⇒ **热检测不跑**；
- `:642 hot = nh = 0` ⇒ 输出 `hot:0`，`:842 strbuf_commit` 返回 **`ACS_OK`**。
期望推翻：类型错应致命（对照 `:719-722` 对 `data_base64` 的处理）。
结果：**返回成功 + 检测关闭 + 恒等帧**，与「检测跑了零坏点」不可区分。**推翻成立，这是本片最强的 fail-open 反例。**

### R3（推翻 S-2：恒真门）—— **推翻成立**
构造：把 `tests/p1cos/CMakeLists.txt:42` 的组名 `properties` 改为 `property`（一次纯维护性改动，不触碰任何科学代码）。
- `p1cos_test_main.hpp:126` 循环中无一组匹配 ⇒ 一条断言都不执行；
- `:138 if (total_fail == 0)` ⇒ **打印 `P1COS TESTS PASS`、return 0**；
- ctest 把 `p1cos_properties` 记为 **PASSED**。
期望推翻：错组名应报错退出。结果：**推翻成立**。

### R4（推翻 S-3：自检无法识别恒真断言）—— **推翻成立**
构造：把 `core.cpp:81` 改为 `P1COS_CHECK(cs, true, "const_field_bitwise")`（恒真，零判别力）。
- 注入 `const_field_bitwise` ⇒ `test_main.hpp:74` 命中 ⇒ `++failures` ⇒ rc=1；
- `selfcheck.cpp:127-135` 判「必败验证通过」。
期望推翻：selfcheck 应识别出这条断言是恒真门。结果：**推翻成立**——注入机制只验证「计数被翻了」，不验证「缺陷真的被注入了」。对照：同片 `badcol.cpp:102,133` 的 `g_inj` 是**真注入**（改被测数据）并配 `:133-136` 非退化对照，仓内已有正确范式，core 未采纳。

### R5（推翻 B-1：租约泄漏可达）—— **推翻成立**
构造：`width=10000,height=10000,f64` ⇒ `plane_bytes = 1e8 × 8 = 800 MB`；`:715/:726` 的 light/pdark/pbias 已驻留。
- `:765` 取租约成功（返回 0）⇒ `lease_active=1`，`:769` 已改写全局 ICV；
- `:797` 分配 800 MB 失败 ⇒ 抛 `std::bad_alloc`；
- 逃逸至 `:1161` catch ⇒ 只重置 state ⇒ **租约与 ICV 双双泄漏**。
期望推翻：cancel 宏 `:779-793` 已证明「返回前先还租」是本函数既有范式，OOM 路径不应例外。结果：**推翻成立**。

### R6（推翻 B-0：恒真门）—— **推翻成立**（本轮最强反例）
构造：读 `badcol.cpp:588-592` 的 fixture `dk2` —— 唯一缺陷是相邻列 12/13 各 `−400`，构成 len=2 的**宽段**；`sci3`（`:614-618`）是列 40-42 各 `−500`，宽段 len=3。
期望推翻：`:610 wide_cut.nd == wide_ok.nd` 与 `:635 s_cut.ns == s_ok.ns` 应是有效非退化对照。
结果：两侧**都没有单列缺陷** ⇒ 无 `MASK_REPAIRED` 列 ⇒ `nd=0`/`nd=0`、`ns=0`/`ns=0`。**这是 `0 == 0`，恒真。** 加重：`:582 apply_wide_budget` 只写 `MASK_WIDE` 位，结构上不可能改动 REPAIRED 计数。**推翻成立。**

### R7（推翻 S-8：代数恒等式）—— **推翻成立**
构造：对 `a.n == a.ns + (a.n - a.ns)` 做整数穷举。
```
a ∈ [-100,100] × ns ∈ [-20,20] = 8481 组 → 反例 0 个
```
期望推翻：该断言应携带信息。结果：恒等于 `a.n == a.n`（除 signed overflow UB），**任何取值都真**。**推翻成立。**

### R8（推翻「坏列逐位断言是恒红门」）—— **推演不成立（子代理构造，我采信其否证结论）**
构造：`badcol.cpp:130` 要求修复值逐位等于 `0.5f*(a+b)`，而生产 `cosmetic_corrector.cpp:502-504` 走 `vl + (vr-vl)*(1.0f*inv)`，代数等价、舍入路径不同。
期望推翻：这是恒红门最典型的藏身处。
结果：**0/2,000,000 组随机 float32 对出现差异**（同指数段内 `b-a` 由 Sterbenz 引理精确可表示、`*0.5` 精确）。**推演不成立，不列入发现。**

### R9（未推翻：「恒红门」全片专项搜索）—— **未找到**
构造：逐条检索本片断言是否存在「两个量逐位相同却去判 ≤ 某阈值」的恒红门。
结果：**本片未发现恒红门**。子代理把 `cosmetic_corrector.cpp` 五个被测函数整体移植到 float32 仿真、重放 badcol 全部 10 个 case 的 **63 条断言 ⇒ 全绿、零恒红**；core 侧亦逐条推演无恒红。
**此项如实记录为「未找到」，不凑数。** 本片的灯绿风险集中在 B-0/S-8/S-9/S-1 的**空洞**，不在灯红。

---

## 6. 盲复算

遮住子代理结论，独立重新取证（本人只读原文 + 只读命令）：

| 待验项 | 子代理结论 | 本人独立取证 | 裁定 |
|---|---|---|---|
| 租约泄漏 | 阻断，`:797` bad_alloc → `:1161` catch 只重置 state | 读 `module_entry.cpp:752-776 / 795-800 / 824-831 / 1160-1170` 四段原文 + `executor.h:1-20,55-65` 契约原文；亲自走通 acquire→alloc→release→catch 链路 | **一致（采纳）** |
| manifest 类型 fail-open | 子代理未报（不在其 4 文件内） | **本人独立发现**，读 `:731-740` 与 `:717-725` 对照 + `integration.json:64` 合同 + `cosmetic_corrector.cpp:243` legacy 收 NULL 四方串起 | **本人新增，非复述** |
| oracle 自洽 | 阻断 B1，`:54-78` 与生产 `:124-155` 逐行同源 | 亲自并排读两段原文，字符级对应（`1.482602218505602`→`1.482602218505602f`、`dark[i] > thr`→同） | **一致（采纳）** |
| σ≡0 退化 | 阻断 B2，六夹具 mad=0 | 亲自读 `fixtures.hpp:81,151,192,237,301,340`（常量基底）+ `:100-126`（≤9 spike）+ `core.cpp:55-90`（32×32）；自行推算 >99% 零偏离 ⇒ MAD=0 | **一致（采纳）** |
| 零组匹配恒真门 | 阻断 B6，`test_main.hpp:126,138-141` | 亲自读 148 行全文并自行构造 R3 推演 | **一致（采纳）** |
| `cc_*` 退役通道 | 「不在任何构建、零消费者」 | **亲自 grep 全仓**：`cc_correct_median/cc_detect_hot/cc_detect_cold/cc_last_error` 的命中**全部落在 `docs/**` 与 `lib/algorithms/calibration/cpp/` 自身定义/头**，**无任何生产代码或 Python 调用者** | **一致（采纳），但比子代理更严**：确认是**真退役**，非「证伪的退役声明」 |
| `-ffast-math` 优化掉 NaN 检查 | 否决 | 亲自 grep 全仓构建文件：零命中 | **一致（采纳否决）** |
| 私建线程池（FORBID-003） | 否决，7 类原语零命中 | 亲自 grep `std::thread\|std::async\|pthread_create\|thread_pool\|hardware_concurrency\|std::mutex` 于 cosmetic+calibration：**零命中**；`:769/:828` 是 OpenMP ICV 注入与还原，非私建池 | **一致（采纳否决）** |
| adapter 测试覆盖 | 「p1cos 不链 DLL」 | 亲自 grep `acsd_p1_cosmetic` 于 `tests/p1cos/CMakeLists.txt`：**零命中**；全仓仅根 `CMakeLists.txt:366-367,427` 与 `install_layout.cmake:155` 提及，均非测试 | **一致（采纳）** |

**盲复算总体裁定：本片 2 条阻断中，1 条（租约泄漏）与子代理一致，1 条（fail-open）为本人独立发现；未见「偏松」项；对子代理的 9 项否决全部采信。未见偏严项——无一条结论依赖不可复现的推断。**

---

## 7. 子代理派发记录

**派发 4 个子代理**（要求：逐行读原文、红队姿态、零 git 写、不编译不跑二进制、不改仓内文件、每条结论给 `文件:行`、必须列「否决的怀疑」）。

| 子代理 | 分片 | 返回 | 复核方式 |
|---|---|---|---|
| #1 | oracle.hpp / fixtures.hpp / test_main.hpp / tests_main.cpp（4 份 767 行） | 已返回详报 | 抽读 `:54-78`、`:100-235`、`:55-125` 逐条复核 |
| #2 | p1cos_tests_badcol.cpp / p1cos_tests_core.cpp（2 份 1239 行） | **已返回详报** | 本人抽读并**独立复核其两条恒真门与代数恒等式**（读 `:583/:588-592/:605-612/:614-618/:630-638` + 整数穷举） |
| #3 | README.md / module.yaml / memory.md / integration.json（4 份 487 行） | **已返回详报** | **本人已先于派发亲自全文读完这 4 份**；另独立复核其 `.pyc` 悬空与 registry 双轨（`git ls-tree` / `git ls-files` / `ls __pycache__` + `module_ports.registry.json:150-196`） |
| #4 | CMakeLists ×2 / .map / .def + 跨仓符号三向对账 | 已返回详报 | 逐条复核 executor 契约、构建对账、私建池扫描；并把其租约泄漏结论**从「资源泄漏」升级为阻断** |

**4 个子代理全部返回，覆盖本片 18 份中的 13 份（4307 行中的 2493 行）；其余 5 份小文件（types.h / module.yaml / memory.md / README / integration.json / tests_main.cpp / .map / .def / 两个 CMakeLists）由本人亲自全文读完并复核子代理结论。**

### 逐条复核与**否决**记录

**采信并加强（4 条）**：
1. **#2 的两条恒真门**（`:610`/`:635`）——我读 fixture 原文确认 `dk2`/`sci3` 只造宽段、**无单列缺陷**，独立判定成立，并补上「`:582 apply_wide_budget` 结构上只写 WIDE 位 ⇒ 即使 fixture 非空仍恒真」这一加重情节。
2. **#2 的代数恒等式**（`:583`）——我用整数穷举独立证明（8481 组零反例），采信。
3. **#4 的租约泄漏**——我把它从「资源泄漏」**升级为阻断**，理由是我另行核实 `executor.h:8` 的进程级不变量表述，使后果从「本模块预算扣减」上升为「全进程 cpu_heavy 模块连带失效」。
4. **#3 的 `.pyc` 悬空与 registry 双轨**——我亲自跑 `git ls-tree`/`git ls-files`/`ls __pycache__` 三条命令确认源码确实不存在、只剩未跟踪 `.pyc`；并亲自 `sed` 出 `module_ports.registry.json:150-196` 与 `module_adapters.cpp:2821/:13916` 确认双轨与在线进程内路径。

**部分否决（1 条）**：
- **#1 的 B2「全部夹具 `mad≡0`」**——我**采信其结构性主张**（常量基底 + 稀疏 spike ⇒ 零偏离过半 ⇒ MAD=0），由我在 `fixtures.hpp:81,151,...` 与 `core.cpp:102`（32×32）上独立重算确认；但**不予背书其手工代入 `check_i6` 的具体阈值数值**（该处依赖手工计算），只采信可独立复算的结构性部分。

**采信其自我否决（1 条）**：
- **#3 推翻了自己**：「p1cos 测试目录只剩 badcol 一个 cpp，测试根本构建不了」——系用 `ls | head` 截断输出误判。完整列举显示 9 个文件全在且全被引用。**这条自我纠错提高了该代理其余结论的可信度，予以采信。**

**采信其关键阴性结论（1 条）**：
- **#2 的 R8**：把 5 个被测函数整体移植 float32 仿真、重放 63 条断言**全绿、零恒红**，并压测 2,000,000 组 float32 对证明 `:130` 的逐位断言**不是**恒红门。这是本片最有价值的阴性结论——它把风险面正确地定位在「灯绿空洞」而非「灯红」。

**独立否决（非子代理提出，本人查后不成立）**：
- 否决「`acsd_module_query_v1` 导出悬空/是空壳」：`module_entry.cpp:1250` 有真实定义 + `:1237-1247` 完整九操作 vtable + `:813` 实调 `ac_correct_frame`。
- 否决「`-ffast-math` 抹掉 NaN 检查」：全仓零旗标。
- 否决「cosmetic/calibration 私建线程池」：7 类原语零命中。
- 否决「integration.json:129 悬空引用」：`eng/tests/unit/cosmetic_integration_test.cpp` **真实存在**。且其 `:861-893` 有 `master_dark` 非法 base64 → 112、长度不符 → 111 的负面覆盖——**但两条都是「值非法」，恰恰没有覆盖 B-2 的「类型错」，反向印证 B-2 未被测到**。
- 否决「`cc_*` 退役声明被证伪」：本人 grep 全仓，`cc_correct_median/cc_detect_hot/cc_detect_cold/cc_last_error` 的命中**全部落在 `docs/**` 与 `cpp/` 自身定义/头**，**无任何生产代码或 Python 调用者** ⇒ **真退役**，非证伪。（但 `lib/algorithms/calibration/memory.md:209-212` 仍在叙述一条 `_load_cpp_dll()` 优先 C++/fallback Python 的**运行期通路**，而该函数代码零命中 ⇒ **悬空引用**。）

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线
git -c core.quotepath=false rev-parse HEAD          # 期望 850a9ede...

# 1) 覆盖率分母（本片 18 份 4307 行，与片清单 :85-87 吻合）
for f in lib/algorithms/cosmetic/src/module_entry.cpp \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_badcol.cpp \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_core.cpp \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_fixtures.hpp \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_oracle.hpp \
  lib/algorithms/cosmetic/README.md \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_selfcheck.cpp \
  lib/algorithms/cosmetic/integration/acsd_p1_cosmetic.integration.json \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_test_main.hpp \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_perf.cpp \
  lib/algorithms/cosmetic/CMakeLists.txt \
  lib/algorithms/cosmetic/tests/p1cos/CMakeLists.txt \
  lib/algorithms/cosmetic/include/acsd/cosmetic/types.h \
  lib/algorithms/cosmetic/memory.md \
  lib/algorithms/cosmetic/module.yaml \
  lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_main.cpp \
  lib/algorithms/cosmetic/src/module_exports.map \
  lib/algorithms/cosmetic/src/acsd_p1_cosmetic.def ; do
  printf "%6d  %s\n" "$(wc -l < "$f")" "$f"; done | awk '{s+=$1} END{print "TOTAL",s}'   # 期望 4307

# 2) 【B-1】租约泄漏：acquire 在 765、分配在 797、release 在 826、catch 只重置 state 在 1161
sed -n '765p;797p;826,829p;1161,1169p' lib/algorithms/cosmetic/src/module_entry.cpp
sed -n '8p;60p' lib/include/acsd/core/executor.h

# 3) 【B-2】fail-open：可选平面类型错被静默跳过；对照必需平面严格
sed -n '731,740p' lib/algorithms/cosmetic/src/module_entry.cpp
sed -n '717,722p' lib/algorithms/cosmetic/src/module_entry.cpp
sed -n '64p;78p' lib/algorithms/cosmetic/integration/acsd_p1_cosmetic.integration.json
sed -n '243p' lib/algorithms/calibration/src/cosmetic_corrector.cpp

# 4) 【S-1】oracle 与生产逐行同源（并排看两段）
sed -n '54,78p' lib/algorithms/cosmetic/tests/p1cos/p1cos_oracle.hpp
sed -n '124,155p' lib/algorithms/calibration/src/cosmetic_corrector.cpp

# 5) 【S-1】六处暗场常量基底 ⇒ MAD=0
grep -n "dark.assign(npix, 5.0f)\|bias.assign(npix, 3.0f)" lib/algorithms/cosmetic/tests/p1cos/p1cos_fixtures.hpp

# 6) 【S-2】零组匹配与全通过不可区分
sed -n '126p;138,141p' lib/algorithms/cosmetic/tests/p1cos/p1cos_test_main.hpp
grep -n "add_test" lib/algorithms/cosmetic/tests/p1cos/CMakeLists.txt

# 7) 【S-3】注入不短路被测代码
sed -n '74,79p' lib/algorithms/cosmetic/tests/p1cos/p1cos_test_main.hpp
sed -n '102p;133,136p' lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_badcol.cpp

# 8) 【S-5】types.h 词表宏零消费者
for m in ACSD_COS_MAN_KEY_MASTER_DARK ACSD_COS_CFG_KEY_HOT_SIGMA ACSD_COS_METHOD_MEDIAN; do
  echo "$m -> $(git -c core.quotepath=false grep -rn "$m" -- '*.cpp' '*.c' '*.hpp' | grep -vc types.h)"; done

# 9) 【S-6】adapter 在自家测试台零覆盖
git -c core.quotepath=false grep -n "acsd_p1_cosmetic" lib/algorithms/cosmetic/tests/p1cos/CMakeLists.txt || echo "NO MATCH"

# 10) 【S-4/S-7】README 过期与根库行号
sed -n '6p;13p;25p;27p;28p' lib/algorithms/cosmetic/README.md
git -c core.quotepath=false grep -n "add_library(acsd_calibration" CMakeLists.txt

# 11) 悬空引用核实：calibration/memory.md 叙述的 _load_cpp_dll 通路在代码中不存在
git -c core.quotepath=false grep -rn "_load_cpp_dll" -- .    # 期望仅 memory.md 命中，零代码

# 12) 否决项复核：私建线程池 / fast-math / cc_* 退役（均期望零命中）
git -c core.quotepath=false grep -rn "std::thread\|std::async\|pthread_create\|thread_pool\|hardware_concurrency" -- lib/algorithms/cosmetic lib/algorithms/calibration
git -c core.quotepath=false grep -rn "ffast-math\|funsafe-math-optimizations\|ffinite-math-only" -- '*CMakeLists.txt' '*.cmake'
git -c core.quotepath=false grep -rn "cc_detect_hot" -- '*.py' '*.c' '*.cpp' | grep -v "calibration/cpp/"   # 期望零命中

# 13) 【B-3】README 的状态锚点工具已被物理删除，只剩未跟踪 .pyc（三条命令须同时成立）
git -c core.quotepath=false ls-tree -r HEAD --name-only | grep -c "check_module_map.py"   # 期望 0
git -c core.quotepath=false ls-files | grep check_module_map                              # 期望不在版本控制
ls -la eng/tools/quality/__pycache__/ | grep check_module_map                              # 期望只剩 .pyc

# 14) 【B-4】全局注册双轨（生产事实 vs 文档声明）
sed -n '150,196p' lib/infrastructure/pipeline/module_ports.registry.json
sed -n '5p;27,28p;63,64p' lib/algorithms/cosmetic/module.yaml
git -c core.quotepath=false grep -n "p1_op_cosmetic" -- lib/infrastructure/scheduler/src/module_adapters.cpp

# 15) 【B-0】恒真门：fixture 只造宽段、无单列缺陷 ⇒ nd/ns 两侧恒 0
sed -n '588,592p;614,618p' lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_badcol.cpp   # dk2 / sci3 只减列
sed -n '610p;635p' lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_badcol.cpp           # 0==0 的比较

# 16) 【S-8】代数恒等式：穷举证明反例为 0
python3 -c "
bad=0
for a in range(-100,101):
    for ns in range(-20,21):
        if a != ns + (a - ns): bad+=1
print('counterexamples:', bad)"   # 期望 0 ⇒ 恒等于 a==a

# 17) 【S-9】core.cpp:242 两侧均已被钉死
sed -n '237p;241p;242p' lib/algorithms/cosmetic/tests/p1cos/p1cos_tests_core.cpp

# 18) 【B-2 加重】输出 manifest 无「母版是否到位」标志 ⇒ 缺席与真零缺陷产出逐字节相同
sed -n '629,657p' lib/algorithms/cosmetic/src/module_entry.cpp
sed -n '2866p' lib/infrastructure/scheduler/src/module_adapters.cpp   # 兄弟路径明文要求「必须留痕」
```

---

## 9. 遗留未决（UNRESOLVED）

| # | 事项 | 影响 | 所需输入 |
|---|---|---|---|
| U-1 | README 的 `NOT_IMPLEMENTED` 与 `acsd.product.json:15` 的 `IMPLEMENTED`、`module.yaml:19` 的 `CONTRACT_READY` 三方冲突 | 模块对外申报状态不唯一 | 项目负责人裁定机读正本；不属本片可自决 |
| U-2 | `lib/algorithms/calibration/cpp/cosmetic_corrector.{cpp,h}`（338 行，零构建零消费者）是否按 AGENTS.md §6「删除或写明原因」处置 | 退役代码留存；且与生产同名不同实现，易被后续 agent 误引 | 需 calibration 片 ALG-calibration-001 同步裁定（该两文件属其成员） |
| U-3 | 本片 1266 行 adapter 的测试面在 `eng/tests/unit/cosmetic_adapter_test.cpp`（不在本片），其是否覆盖 B-1 的 OOM 路径与 B-2 的类型错路径未知 | 若已覆盖则两条降级为须修 | 需覆盖 `eng/tests/unit/` 的分片确认 |
| U-4 | `check_module_map.py` 已被 `e5f589a6` 删除，但 README:4/8 仍以其输出为状态依据 ⇒ 机读状态判定依据**物理上已不存在** | 模块对外申报状态无有效来源 | 需负责人裁定机读正本；不属本片可自决 |
| U-5 | `p1cos_tests_core.cpp:359-364` 断言 `n_hot==0`，其成立与否取决于 `std::nth_element`（`cosmetic_corrector.cpp:39`）对 NaN 的放置位置——**C++ 未指定行为**（`operator<` 对 NaN 返回 false）。按纪律本人未编译未运行，无法给出确定答案 | 该锚换 STL/平台可能翻红，而翻红与实现对错无关 | 需前台实机跑 `p1cos_negative` 组复核，并记录所用 STL/编译器版本作为该锚的前置条件 |
| U-6 | `p1cos_tests_badcol.cpp:330` 援引 `DISP-COS-009` 作为「dark/bias=NULL ⇒ 恒等 pass、零改动」的依据，但 `cosmetic_corrector.cpp:547` 明写该 DISP **已作废注销**，而 `COSMETIC_ALGORITHMS.md:481` 又将其列为在册条款（内容已改写） | 测试援引的是被多个文件共同宣告已注销的那一版语义 | 需 SCI/控制包车道裁定 DISP-COS-009 的现行状态 |
