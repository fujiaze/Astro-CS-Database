# T06 第 1 轮订正 · 子代理 D（`architecture/*`、`api/abi/*`、`UNIFIED_OBJECTS.md`）

白名单：`docs/engineering/architecture/*.md`、`docs/engineering/api/abi/*.md`、`docs/engineering/UNIFIED_OBJECTS.md`。
全程无 git 写操作，git 只读且一律 `git -c core.quotepath=false`。未越界，未新建仓内文件。
**实际改动 6 份**（全部在白名单内）：`architecture/MODULE_MAP.md`、`architecture/ARCHITECTURE.md`、`architecture/DATA_FLOW.md`、`api/abi/ABI.md`、`api/abi/SECURE_LOADER.md`、`UNIFIED_OBJECTS.md`。

---

## 1. 逐条处置表

### P0 · 节点序列与构建图登记

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| 1-11 / 8-3 | **已改** | `MODULE_MAP.md:62` 「`normalize` 八节点（校准、修饰、**星点与 PSF、天体定位**、测光、噪声信噪比、球面重采样、产品写出）」 | 「`normalize` 八节点（校准、修饰、**天体定位、星点与 PSF**、测光、噪声信噪比、球面重采样、产品写出）」 | `python3 -c "import json;d=json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'));print([m['module_id'] for m in d['modules']][:8])"` → `['acsd.phase1.calibration', 'acsd.phase1.cosmetic', 'acsd.phase1.wcs-platesolve', 'acsd.phase1.star-psf', 'acsd.phase1.photometry', 'acsd.phase1.noise-snr', 'acsd.phase1.drizzle', 'acsd.phase1.writer']`；`grep -n "pos(psf)\|pos(wcs)" docs/engineering/contracts/PIPELINE_BLOCK.md` → `:107 \| C6 psf 在 wcs 之后 \| pos(psf) > pos(wcs)…`。**按语义改，未动判据编号**（仍写 `C6`，编号改名是别人的车道）。与 `ARCHITECTURE.md:63` 的生产节点序 `plate_solve → detect_sources` 一致，无冲突。 |
| 2-6 / 8-4（路径表） | **已改** | `MODULE_MAP.md:64` 路径列 `…`、`lib/algorithms/calibration`、`lib/algorithms/cosmetic` | 末尾补 `、`lib/algorithms/noise_snr``（职责列本来就写了「噪声模型」） | `grep -n "acsd_phase1_noise" CMakeLists.txt` → `:1105 add_library(acsd_phase1_noise STATIC`、`:1106-1108` 源为 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` / `wrapper_phase1/snr_frame_science.cpp` / `cpp/src/snr_science.cpp`；`grep -rn "noise_snr" CMakeLists.txt` → `:362 add_subdirectory(...)` 为**注释行**、`:1106` 起为源集直编 |
| 2-6 / 8-4（非交付面行） | **已改** | `:92` 「`lib/algorithms/noise_snr` \| 不在根构建图（未 `add_subdirectory`），故构建图不引用」 | 「`lib/algorithms/noise_snr` \| 子目录未 `add_subdirectory`，其源集由根构建图直接编入 `acsd_phase1_noise` 目标（静态库，随链接闭包进产品图）；该子目录自带 target 不在根图」 | **行号以实际 grep 为准**：`CMakeLists.txt:1105` 声明目标、`:1106-1108` 列源、`:1245`/`:1349` 进链接闭包。`lib/infrastructure/scheduler/src/module_adapters.cpp:14` 头注「noise-snr → NoiseModel::estimate (lib/algorithms/noise_snr/wrapper_phase1)」佐证该路径在役 |
| 2-16 | **已改** | `:15` 「`lib/infrastructure/scheduler`（target 定义在根构建）」 | 「`eng/tests/conformance/noop`（target `acsd_noop` 在该子目录声明，由根构建图 `add_subdirectory` 引入）」 | `grep -n 'conformance/noop' CMakeLists.txt` → `:324 add_subdirectory(eng/tests/conformance/noop)`；`eng/tests/conformance/noop/CMakeLists.txt` 内 `add_library(acsd_noop SHARED src/noop_module.c)`。**证据锚列未动**（已核 `eng/packaging/acsd.product.json` 的 `MOD-NOOP` = `acsd.conformance.noop`、`rel_path = modules/acsd_noop.so`，与改后路径自洽） |
| 2-17 | **已改** | `:79` 「**在役 registry = `p3_proj.h`/`p3_proj.cpp`**，可作产品声明的投影以 `p3_projection_registry.h` 的当前声明为权威」 | 「**在役 registry = `p3_projection_registry.h`**（header-only inline，由同目录 `p3_wcs.cpp` 直接包含），可作产品声明的投影以该注册表的当前声明为权威」；并在 `:94` 新增非交付面行「`lib/algorithms/projection` 的 `p3_proj.h` / `p3_proj.cpp` \| v6 内核行：无构建目标编译其源文件，不进产品图……」 | **先确认真实文件名**：`ls -1 lib/algorithms/projection/` → `p3_projection_registry.h` 存在；`grep -n "include" lib/algorithms/projection/p3_wcs.cpp` → `:7 #include "p3_projection_registry.h"`；`p3_projection_registry.h:164 inline P3WcsStatus p3_proj_declare(...)` 证实 `p3_wcs.cpp:68` 调用的 `p3_proj_*` 符号来自该 header。`grep -rn 'p3_wcs.cpp\|p3_proj.cpp' --include=CMakeLists.txt lib/algorithms/projection/` → 只有 `:19 add_library(acsd_p3_projection_wcs STATIC …/p3_wcs.cpp)`；全仓 `p3_proj.cpp` 的编译只出现在 `run/FINAL-07*/` 归档快照，不在生产图。**原判成立** |
| 8-13 | **已改**（状态归治理面，本篇只留映射） | `:52-55` 「- 运行期宿主接线尚未闭合，收敛判据是本节的可达性核对项；收缩可达面须经 …变更流程。」+「- 相邻事实：`eng/packaging/acsd.product.json` 自述平台运行时与 I/O 平台单元尚未落实现；`lib/infrastructure/pipeline/module_loader/README.md` 自述宿主注册表接线属平台运行时工单。」 | 「- 可达面收缩须经 `docs/detail/registry/` 的变更单，按最高设计的顶层结构章走变更流程。」+「- 本篇只承载模块到目录、产物与证据锚的映射。平台运行时与 I/O 平台单元的实现状态，声明面是 `eng/packaging/acsd.product.json` 与 `lib/infrastructure/pipeline/module_loader/README.md`，状态类结论由 `../governance/UNRESOLVED.md` 承载。」 | **未删真内容**：两个来源路径与变更流程规则全部保留，只把「尚未闭合 / 尚未落实现 / 工单」这三条状态声明改成指向其声明面。`governance/` 不在我白名单，**只登记不写**，见 §4 |

### P0 · 平台口径冲突

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| 2-1 / 8-10 | **已改（向最高设计对齐）** | `:123` 「正式开发、客户端与发布平台是 Windows x64，兼容下限 Windows 10 22H2 x64；Linux amd64 承载常在线控制、静态分析、轻量编译与小合成实验。」+ `:125` 「…Linux 同源产出 `acsd` 加 `.so` **技术预览用于轻验证，其读数不作为 Windows 发布性能结论**。」 | `:123` 「交付平台是 **Windows 10+ amd64 与 Linux amd64 两个**。运行时、I/O、科学模块与 CPU provider 作为动态库随包交付：Windows 用户面对 `acsd.exe` 加各 `dll`，Linux 用户面对 `acsd` 加各 `so`；两平台各给出一个解压目录（唯一可执行文件、各动态库、schemas、配置）。」+ `:124` 「**开发、构建、合成与真实数据终验在 Linux amd64 节点上完成**，随后交 Windows 复验，再由负责人终审发布候选。」+ `:126` 「**Windows 官方工具链为 MSVC**；球面浏览器…」 | `sed -n '455,480p' docs/ACSD_DESIGN.md` → 第 11 章「双平台发行」表两行均为交付平台：`Windows 10+ amd64 \| acsd.exe \| 解压目录：唯一 exe + 各 dll + schemas + 配置`、`Linux amd64 \| acsd \| 解压目录：唯一 ELF + 各 so + schemas + 配置`；`:466` 流程图节点 `L["Linux 节点：开发·构建·合成·真实数据终验"] → CI["双平台 CI"] → W["Windows 复验"] → R["发布候选 · 负责人终审"]`。`ARCHITECTURE.md:3` 自认「冲突时以最高设计为准」。 |
| 2-1 的另一半 | **已改** | 「兼容下限 **Windows 10 22H2 x64**」 | 「Windows **10+** amd64」 | `grep -rn "22H2" docs/` → 改前**仅此一处命中**，无任何权威来源支撑 22H2 这一收窄；最高设计写的是「Windows 10+ amd64」。删除无据的收窄不是删真内容。 |

**保留未动的原文事实**：动态库随包交付模型、解压目录交付物、「球面浏览器不入 manifest」「未来图形界面经命令行/JSON/退出码/产品文件调用」——全部按最高设计第 10、11 章措辞保留在改后段落里。

### P0 · 精度归属

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| 2-5 | **已改** | `UNIFIED_OBJECTS.md` `frame_snr` 行精度列 `float32\|float64` | `float64` | `sed -n '155,167p' docs/ACSD_DESIGN.md` → 「### 3.3 精度归属 / - 稠密大面（图像面、球面累加器、方差/覆盖面、HiPS tile）默认单精度；/ - **稀疏与元数据（帧级信噪比**、WCS 解、星表匹配、测光定标、manifest）全程双精度；」。`docs/science/unified/DATA_SEMANTICS.md:141-145` §3.8 精度表逐项复述同一归属：「稀疏与元数据 \| FP64 \| 帧级信噪比、WCS 解、星表匹配、测光定标、控制点」。`frame_snr` 即「帧级信噪比」⇒ FP64。 |
| 2-4 | **已改** | `provenance` 行精度列 `integer` | `非数值元数据（字符串 / 键值：来源链、输入哈希、运行标识、产品类型）` | `python3 -c "import json;d=json.load(open('eng/contracts/schemas/unified/provenance.schema.json'));print(list(d['properties'].keys()))"` → `['unified_object','object_schema_id','schema_version','units','missing_value','precision','object_weight_capability','object_weight_verdict','provenance','input_hashes','run_id','product_type_id','unavailable','k_corr']`，全部是字符串/键值/对象，无数值面 ⇒ `integer` 语义不成立。 |
| U-7（2-13） | **如实登记在文档内 + 转 UNRESOLVED** | 无 | 新增「**精度列的读法**」与「**精度面当前的实现缺口（需负责人裁决）**」两段 | `sed -n '32,51p' lib/infrastructure/aio/src/aio_api.cpp` → `:35 static int g_aio_precision_mode_fp64 = 0; // 0=FP32, 1=FP64`、`:37 AIO_EXPORT void aio_set_precision_mode(int is_fp64)`、`:49 extern "C" int aio_internal_is_fp64()`；`:33` 头注「PrecisionContext 单例在 DLL 边界不共享 (EXE 和 DLL 各有一份副本), 必须通过 `aio_set_precision_mode` 显式设置」。**确认：单个全局位无法表达「dense=f32 且 sparse=f64」这一组合**。另注：13 个 canonical schema 的 `precision` 属性枚举一律是 `['float32','float64','integer']`（逐个 dump 核过），`provenance` 的 `integer` 在 schema 侧合法、在语义侧无意义。 |

**登记文本（已逐字写入文档）**：「本表按对象语义标注的是**应归属**的精度，不是当前机器已强制执行的精度；『拆成两个独立精度量』还是『把最高设计的精度归属收敛为一个全局模式』属未决项……裁决前本列不得被读作运行时保证。」——**未假装两者已统一实现**。

### P0 · ABI 层与命名

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| 8-9 | **已改** | `:5` 「**单一头** `lib/include/acsd/common_abi_v1.h`，C 与 C++ 双可编译；跨边界不出现标准库类型、异常、运行时类型信息与编译器私有类型。」 | 「公共 C ABI 基础层有**两套并行头**……：- 模块化头族 `lib/include/acsd/abi/`：`status_codes.h` ← `artifact_api_v1.h` ← `host_api_v1.h` ← `module_api_v1.h`，依赖单向、每头独立可编译；- legacy 单头 `lib/include/acsd/common_abi_v1.h`。两者共享基础类型名……与数值语义，v1 可互操作；差别是模块化头族多出 `ACS_ERR_EXCEPTION = 10`。`status_codes.h` 明文规定二者**不得在同一翻译单元混合 include**，收编路径是随模块迁移退役 legacy 单头、迁到模块化头族。」 | `ls -1 lib/include/acsd/abi/` → `artifact_api_v1.h  host_api_v1.h  lifecycle_v1.h  module_api_v1.h  status_codes.h`；`ls -1 lib/include/acsd/` → `abi  common_abi_v1.h  contracts  core  io  README.md`（**两套并存，确认**）。`sed -n '26,30p' lib/include/acsd/abi/status_codes.h` → 「二者不得在同一 TU 混合 include（legacy 随模块迁移退役, ABI-002/006 处理收编）」。 |
| 2-2 | **已改（且推翻审稿前提，见 §2）** | 文档代码块止于 `ACS_ERR_BUDGET=8, ACS_ERR_SELFTEST=9` 后接 `} acsd_status;` | 追加 `,` + `ACS_ERR_EXCEPTION=10,   /* 模块化头族新增：DLL 边界捕获的 C++ 异常转换值 */` + `ACS_ERR_INTERNAL=70     /* 未分类; 与进程退出码 INTERNAL=70 同语义 */` | `grep -n "ACS_ERR" lib/include/acsd/abi/status_codes.h` → `:164 ACS_ERR_EXCEPTION = 10`、`:165 ACS_ERR_INTERNAL = 70`；`grep -n "ACS_ERR_INTERNAL\|ACS_ERR_SELFTEST" lib/include/acsd/common_abi_v1.h` → `:65 ACS_ERR_SELFTEST = 9`、`:66 ACS_ERR_INTERNAL = 70`。`cat lib/infrastructure/cli/exit_codes.h` → `INTERNAL = 70`。**两套头都有 INTERNAL(70)，代码无缺项，缺的是文档**。 |
| 2-3 | **已改** | `:15` 「- 前缀 `acs_`（函数）/ `ACS_`（类型 / 常量）；所有 struct 首两字段 …」 | 「- 类型一律 `acsd_*` 小写前缀：`acsd_head`、`acsd_span_f32`、`acsd_span_f64`、`acsd_span_u8`、`acsd_handle`、`acsd_status`、`acsd_allocator`、`acsd_logger`、`acsd_cancel`、`acsd_thread_budget`；/ - 枚举常量与宏一律 `ACS_*` 全大写：`ACS_ABI_VERSION_V1`、`ACS_ERR_*`、`ACS_LOG_*`、`ACS_SPAN_F32`；/ - 导出函数一律 `acsd_*` 小写前缀，由模块化头族的 `ACSD_CALL` / `ACSD_EXPORT` 标注调用约定与可见性；/ - 所有跨边界 struct 首两字段 …」 | 示例全部抄自真实符号：`common_abi_v1.h:31/35`（`acsd_span_f64`）、`:48`（`ACS_SPAN_F64`）、`:55-67`（`acsd_status` 枚举）、`status_codes.h:44-52`（`ACSD_CALL` / `ACSD_EXPORT`）、`status_codes.h:155-165`（`ACS_ERR_*`）、`astro_image_io.h:98`（`aio_set_precision_mode` 为 `AIO_EXPORT void`）。**原稿写的 `acs_` 函数前缀在两套头里零命中**，属凭空规则。 |
| 1-9 / 9-12 | **已改（整段重写）** | `:77-79` 「`acsd_status` 是 ABI 层的粗粒度返回码，只区分「成功 / 哪一类失败」；/ 面向用户的分类、退出码与阶段 ID 分别由 「并发合同模板」一节 / 「头文件独立性验证合同」一节 / 「落点映射」一节/ 与 「落点映射」一节 定义，两层不得互相替代。」 | 「`acsd_status` 是 ABI 层的粗粒度返回码，只区分「成功 / 哪一类失败」：它不承载面向用户的分类，也不承载阶段 ID。/ 面向用户的失败分类与退出码由最高设计的「机器输出与退出码」节定义并在命令行层收敛，两层不得互相替代。/ 进程退出码 `INTERNAL = 70` 与 `ACS_ERR_INTERNAL = 70` 是同一个「未分类内部错误」语义的两层表达，值域相同不是巧合，不得各自另立映射表。」 | `grep -n "^#\{2,3\} " docs/ACSD_DESIGN.md` → `:346 ### 7.2 机器输出与退出码`、`:352 ### 7.3 错误传播与日志`、`:432 ### 8.5 模块与 ABI`、`:440 ## 9. CPU 后端与资源`。原稿的「3 项对 4 个节名」与「A/B/C 与 D 定义」均无对应实体，已按语义重写。 |
| 1-9（落点映射） | **已改** | `:97` 「分阶段 API 面见 `../PUBLIC_API.md`、`../PUBLIC_API.md`、`../PUBLIC_API.md`」（同路径重复 3 次）+ 4 行机械锚 | 8 行落点表，**同一路径只出现一次**，行首改为「本篇条款」而非「本文件」，行内不再用「「X」一节」式锚 | 同上节名清单；`grep -c 'PUBLIC_API.md`' 改前该行 3 次、改后全文 3 次且分属导语/参考文献/落点表各一 |
| 5-15 | **已改** | `:9`、`:16`、`:66` 「同最高设计 **的** 模块与 ABI 条款」（节名为空） | `:3` 「上游：最高设计的「模块与 ABI」与「CPU 后端与资源」两节」；`:15` 「公共 ABI 的命名与版本握手规则同最高设计的「模块与 ABI」节」；`:66` （代码块内注释）→ 移入落点表「命名与版本握手 \| 最高设计的「模块与 ABI」节」 | `grep -n "^#\{2,3\} " docs/ACSD_DESIGN.md` → `:432 ### 8.5 模块与 ABI`、`:440 ## 9. CPU 后端与资源`（**真实节名**） |
| 8-9 的 api 面（README） | **未动（维持审稿判）** | `api/README.md`、`api/abi/README.md` 各 2 行 | — | 复核后确认两者体量与内容均与目录实况相符（`api/` 下 `PUBLIC_API.md` + `abi/`；`api/abi/` 下 `ABI.md`、`SECURE_LOADER.md`），**同意审稿「无缺陷」，不写**。 |

### P1 · 其他

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| 6-1 / 6-2（内部矛盾） | **已改（只消内部矛盾）** | `:9` 命名块行范围「**阶段内节点之间**」；`:22`/`:33`/`:44` 三行中间量形态「**阶段内命名块**」；`:14` 「阶段内节点的交换面是 `output_dir` 文件约定，**不是命名块**」 | `:9` 范围「**单节点执行期**」；`:22`/`:33`/`:44` 三行形态统一为「**单节点执行期的内存对象；节点间交换面是 `output_dir` 文件约定**」 | `python3 -c "import json;d=json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'));print(d['carrier_contract']['statement'][:120])"` → 「**节点间不存在内存块传递**……命名块（PipelineFrame 的 data/variance）只存在于**单节点执行期**的内存中，不落盘、不跨节点、不跨阶段。」`module_adapters.cpp:33-35` 头注同向（节点间 typed artifact 经 `output_dir` 文件约定传递）。**未改 `:14`**（本篇既有立场），只把三行与载体表对齐到它；顶层冲突登记 §4 的 U-1。 |
| 6-1 的失效内部锚 | **已改** | `:11` HiPS 产品树行规范依据「**本文第 4–6 节**」 | 「`../contracts/HIPS_STORAGE_FORM.md`」 | `grep -n "^#\{2,3\} " docs/engineering/architecture/DATA_FLOW.md` → 全文 17 个 `##`，**无任何数字编号节**，原锚不可解析 |
| 3-5 | **已改（补变量定义 + 点明退化关系）** | `:116-117` 式子后无变量定义；`:121` 只定义 `B`；`:125` 未定义 `K` / `num_threads` / `W_eff` | 新增「变量取义：」段，逐项定义 `n`（帧单元数，对照实验写 `n_units`）、`lease`、`frame_workers`、`thread_budget`、`in_flight`、`inner_omp`、整数除法、`MemAvailable`/`W`/`H`/`B`；并补 `K`/`num_threads`/`W_eff` 定义 | 原 `:152` 已用 `n_units`，公式块用 `n` —— 同一篇两种写法，已统一为「`n`，对照实验按帧计档时写作 `n_units`」 |
| 5-10 | **已改** | `:4` `docs/detail/common/UNIFIED_MODEL`（另 `:9`/`:19`/`:30`/`:46`/`:50` 六处同族引用） | `docs/detail/UNIFIED_MODEL.md` 的「数据对象（各自具名）」一节 | `find docs -name 'UNIFIED_MODEL*'` → **只有** `docs/detail/UNIFIED_MODEL.md`；`ls -d docs/detail/common` → 无该目录（`docs/detail/common.md` 是**文件**，不是目录）。`grep -n "^#\{1,3\} " docs/detail/UNIFIED_MODEL.md` → `## 1. 统一线性观测模型` / `## 2. 数据对象（各自具名）` / `## 3. 三类配置严格分离` |
| 5-12 | **已改** | `:1` 标题「UNIFIED_MODEL 「13 个对象 → canonical schema → schema ID」一节 的 **13** 个对象」；`:30` 表头「可否作权重（UNIFIED_MODEL 「13 个对象 → canonical schema → schema ID」一节 原文）」 | 标题「**13 个 canonical 数据对象与 canonical schema 的对照登记**」；表头「可否作权重（上位正本「数据对象（各自具名）」一节原文）」 | **两个旧名都不存在**：`docs/detail/UNIFIED_MODEL.md` 里既无「13 个对象 → canonical schema → schema ID」，也无「weight/value/scale/sigma/snr 歧义映射」（后者是 `docs/engineering/data/ARTIFACTS.md:98` 自己的章节标题，不是 UNIFIED_MODEL 的）。**我选定的表述**（见 §5） |
| 10-3 | **已改（不留不可执行命令）** | `:21` 「机器可复跑断言（`python3 -m unittest discover -s eng/tests/contracts -t eng/tests/contracts`）：」+ `:23-26` 四条 `eng/tests/contracts/test_unified_object_contract.py::…` 方法名 | 四行「判据 \| 人读判据 \| 机器可核事实」表（判据名用自然语言，不再引用不存在的测试方法名）+ 明写「本节不声明执行器……**没有可执行载体**，按人读对抗性审核逐条核对。补执行器属代码侧订正」 | `ls eng/tests` → 只有 `conformance/`、`validation/`；`git -c core.quotepath=false ls-files eng/tests` → 106 项，**全部**在 `conformance/` 与 `validation/` 下。`find . -name 'test_unified_object_contract*' -not -path './run/*'` → 0 命中 |
| 7-1 / 10-1 | **已改** | `:82` `eng/tests/common/jsonschema_min.py`；`:89` `eng/tests/contracts/product_family/`；`:105` `eng/tests/contracts/test_unified_object_contract.py` 与 `lib/algorithms/integration/phase2_integrate/oracle/recon_contract_gate.py`；`:123`/`:133`/`:136` 三处 `test_unified_object_contract.py`；`SECURE_LOADER.md:9-11` `eng/tests/abi/`（`abi003_loader_probe.c` + `test_secure_loader.py`）、`:104` `python3 eng/tests/abi/test_secure_loader.py`、`:111` `eng/tests/abi/test_abi005_echo.py` | 全部改为「判据 + 人读核对 + 无执行器登记」，或改指现存载体（`config_separation_anchors.json`、`unified_object_compatibility_map_v1.json#legacy_contract_id_map`、`sparse_snr_layer.schema.json`、`product_family_field_constraints.schema.json`） | 白名单全量死链扫描（贴 §6）：改前 **5 条仓根相对 + 4 条 CLI 内联**死链，改后 **0 条** |

### 9-x 语言

扫描白名单 `V[0-9]{2,}|R-[0-9]+|P-[0-9]+|已删|旧版|作废|曾|已从|G08-|ACR`，**无流水编号、无历史叙事、无 commit、无日期/版本号元信息块**。残留 5 条命中经逐条核对**全是假阳性**：4 条是 schema `$id` 的 `/v1` 版本串（`…/unified/ivar/v1` 等），1 条是 `§4.1` 章节号。**本车道 9-x 无需改动**。

---

## 2. 我推翻的审稿判定（依据保留，保留原判）

### 2.1 推翻 2-2 的前提：ABI 枚举**不缺项**，缺的是文档

- **审稿原判**：「`ABI.md:38–42` `acsd_status` 枚举止于 `ACS_ERR_SELFTEST=9`，**没有 INTERNAL(70) 的 ABI 层对应**」→ 建议二选一：「补 `ACS_ERR_INTERNAL=70`（与既有 C ABI 枚举不冲突，因值域不同）」或「在 ABI.md 写明 **70 由宿主收敛、ABI 层不表达**」。
- **我的复核**（两条一手证据）：
  - `grep -n "ACS_ERR_INTERNAL" lib/include/acsd/common_abi_v1.h` → `:66  ACS_ERR_INTERNAL = 70   /* 未分类; 等价 CLI 退出码 70 语义 */`
  - `grep -n "ACS_ERR" lib/include/acsd/abi/status_codes.h` → `:164 ACS_ERR_EXCEPTION = 10`、`:165 ACS_ERR_INTERNAL = 70     /* 未分类内部错误; 等价 CLI 退出码 70 语义 */`
- **结论**：两套 C ABI 头**都已经有** `ACS_ERR_INTERNAL = 70`，且都显式注释「等价 CLI 退出码 70 语义」。**审稿的第二个建议（「70 由宿主收敛、ABI 层不表达」）是错的**——ABI 层明确表达它。缺陷是 **`ABI.md` 的代码块是枚举的过期副本**（止于 9，漏了 10 与 70），属纯文档侧。
- **处置**：改文档代码块补齐两项，**不登记为「需代码侧订正」**。
- **我推翻的范围**：审稿「ABI 层无 INTERNAL 对应」这一事实判断；**审稿「ABI.md 与代码不同步」这一定性我保留**，并按它改了文档。

### 2.2 降级 2-3 的「未给函数示例」为「凭空规则」

审稿说 ABI.md 的命名规则「未给任何函数示例」。复核后更严重：规则写的函数前缀 `acs_` 在两套头里**零命中**（真实导出函数是 `acsd_*` / `aio_*` 风格）。建议改法方向正确，我按真实符号落码，并把它从「缺示例」升级为「规则本身与代码不符」。

### 2.3 收窄 5-12：两个节名**都不存在**，不是「同一节两个名字」

审稿判「两篇把 `docs/detail` 同一份 UNIFIED_MODEL 的同一节给了两个不同的节名」。复核 `grep -n "^#\{1,3\} " docs/detail/UNIFIED_MODEL.md` → 该文件只有 3 节（`## 1. 统一线性观测模型` / `## 2. 数据对象（各自具名）` / `## 3. 三类配置严格分离`）。**两个名字都不指向真实节**：「13 个对象 → canonical schema → schema ID」不存在；「weight/value/scale/sigma/snr 歧义映射」是 `data/ARTIFACTS.md:98` 的**本篇自有章节标题**，被误挂到 UNIFIED_MODEL 头上。
⇒ 处置不是「统一两个名字」，而是**按 `docs/detail/UNIFIED_MODEL.md:15` 的真实节名 `## 2. 数据对象（各自具名）` 重新描述**。

### 2.4 否决 8-13 对 `MODULE_MAP.md:31–56` 整段的状态清理

审稿建议「状态归 `governance/UNRESOLVED.md`，本篇只留映射」，方向对，但若照字面删掉 `:52-55` 会**删掉真内容**（`eng/packaging/acsd.product.json` 与 `lib/infrastructure/pipeline/module_loader/README.md` 两个真实声明面 + 变更流程规则）。我的处置：保留两个来源路径与流程规则，**只把「尚未闭合 / 尚未落实现 / 工单」这三条状态断言改写成指向其声明面的映射句**。`governance/` 不在我白名单，**只登记不写**。

---

## 3. 需代码侧订正的问题（我只改文档，不碰代码）

| # | 问题 | 一手证据 | 影响 |
|---|---|---|---|
| C-1 | **`p3_projection.h` 头注与生产事实相反**：`p3_projection.h:5` 自述「本 registry 已退场：唯一在役 registry = v6 线 `p3_proj.h`/`.cpp`」，而 `p3_projection_registry.h:1` 自述「本文件是唯一权威」、`p3_wcs.cpp:7` 实际包含的是 `p3_projection_registry.h`、`CMakeLists.txt:19` 只编 `p3_wcs.cpp` | 已逐条 grep 复核 | 生产事实无歧义，注释是过期的；下次有人按注释改会引回第二权威 |
| C-2 | **`eng/tests/` 判据载体缺失**：`git ls-files eng/tests` = 106 项，**全部**在 `conformance/`（noop 骨架 5 件）与 `validation/`（release02 实验脚本）下。`UNIFIED_OBJECTS.md` 原列的 4 条唯一性判据 + 产品族 Oracle + 共享校验器 + `recon_contract_gate.py` + `SECURE_LOADER.md` 的 3 个 ABI 测试面，**全仓零载体** | `ls eng/tests`、`git -c core.quotepath=false ls-files eng/tests`、`find . -name '<名>' -not -path './run/*'` 均为 0 命中 | 判据仍生效但不可机器复跑；已在文档改为人读并登记 |
| C-3 | **`SECURE_LOADER.md` 验收矩阵无 fixture 载体**：11 条正/负路径的装配 fixture 由谁构建未定义（原稿指向已不存在的 `eng/tests/abi/`） | `ls eng/tests/abi` → 无该目录 | 安全装载器的正本验收不可复现 |
| C-4 | **`provenance` schema 的 `precision` 枚举语义空洞**：`provenance.schema.json` 把 `precision` 列为 required，但枚举只有 `['float32','float64','integer']`，对一个纯字符串/键值对象三者都无意义（`artifacts` 也一样） | `python3` 逐个 dump 13 个 canonical schema 的 `precision` | 我在文档侧按语义标注，schema 侧需负责人定夺是加 `string|keyvalue` 还是把 `precision` 移出 `provenance` 的 required |
| C-5 | **模块化头族的机器检查面失效**：`status_codes.h:12` 自述「机器检查见 `eng/tests/abi/run_abi_checks.sh`」，该路径不存在 | `ls eng/tests/abi` → 无该目录 | ABI 头族的独立编译/layout 断言无执行体；`ABI.md` 已改为「登记在 `status_codes.h` 头注」而不再声称可跑 |

---

## 4. 需权威补充 / 负责人裁决的问题（已登记，未单方面改）

| # | 事项 | 分歧 | 我做了什么 / 需要什么 |
|---|---|---|---|
| **U-7** | **精度归属是两类独立还是单一全局模式** | 最高设计「精度归属」节 + `DATA_SEMANTICS.md` §3.8 要求「稠密大面 FP32」与「稀疏与元数据 FP64」**同时成立**；机器侧 `aio_api.cpp:35` 只有一个全局位 `g_aio_precision_mode_fp64` + `aio_set_precision_mode(int)`，`aio_internal_is_fp64()` 读同一变量 | 我把 `UNIFIED_OBJECTS.md` 精度列按对象语义改正，并在表下**如实登记该缺口**、明写「裁决前本列不得被读作运行时保证」。**需负责人裁决**：拆成两个独立精度量，还是把最高设计的精度归属收敛为一个全局模式并同步订正该节 |
| **U-1** | **命名块是否为阶段内节点间载体** | `AGENTS.md:88` + `ACSD_DESIGN.md:379`（8.2 命名块内存管线与块生命周期）说块跨节点；`PIPELINE_BLOCK.md:17`–`:20` + `DATA_FLOW.md` + 注册表 `carrier_contract` 说块不跨节点、只存在于单节点执行期 | 我**只消除了 DATA_FLOW 内部矛盾**（载体表 + 三行中间量统一到本篇既有立场「节点间交换面是 `output_dir` 文件约定」），**没有改 `:14` 的顶层立场**。**需负责人裁决**：若 U-1 判为「块跨节点」，则 `DATA_FLOW.md:9`/`:22`/`:33`/`:44` 与 `pipeline_block.schema.json` 的 title 须同批回改 |
| **U-3 / 2-1** | **平台角色** | 最高设计第 11 章双平台交付 + Linux「真实数据终验」；原稿把 Linux 降为控制面 | 我**按权威链把 `ARCHITECTURE.md` 向最高设计对齐**（Linux 是交付平台之一并承担真实数据终验）。**若认为最高设计该改，须负责人批准**——我未动最高设计。同时删掉无据的「Windows 10 22H2」下限（全仓仅此一处命中） |
| **U-8** | （审稿已登记，非我车道） | UPM 权重几何因子归属三套公式 | 我未触碰 `DATA_FLOW.md:142`（其式与代码 `upm.h:177` 一致，是**正确的一侧**）。改 `ARTIFACTS.md` / `TRACEABILITY.md` 的人请以 `upm.h` + `DATA_FLOW` 为准 |
| **U-2 / U-4 / U-5 / U-6** | 审稿已登记，`run_manifest` 正本、L2 enforcement、`AstroCsExitCode`、`ACSD-BASS-Index` | 均不在我白名单 | 未动 |

---

## 5. 我否决 / 维持不动的审稿判定

| # | 审稿判定 | 我的处置 | 依据 |
|---|---|---|---|
| N-1 | 「`api/README.md`、`api/abi/README.md` 体量 2 行，判无缺陷」 | **维持原判，不动** | 复核两文件内容与目录实况相符（`api/` = `PUBLIC_API.md` + `abi/`；`api/abi/` = `ABI.md` + `SECURE_LOADER.md`），AGENTS §5「每个文件夹内放一个极简 README」已满足 |
| N-2 | 「`DATA_FLOW.md:90` 声明通用容差正本在 `../testing/TEST.md` **成立**，保留即可」 | **同意，保留未动** | `TEST.md` 不在我白名单；且本轮无新证据推翻 |
| N-3 | 「`3-7` `E = Var_w/Var_opt − 1` 尺度不变性成立，登记为已验证项」 | **同意，未动** | `UNIFIED_OBJECTS.md:98` 该推导原文保留 |
| N-4 | 「`4-7` MAD→σ 常数逐位正确」 | **同意，未动** | 涉 `data/ARTIFACTS.md`，非我车道 |
| N-5 | 「`1-6/1-7` PIPELINE_BLOCK 双 C 编号冲突应改名空间 `PC-C*`/`IR-C*`」 | **不代改** | `contracts/` 不在我白名单；且我在 `MODULE_MAP.md:62` 只按**语义**对齐节点序，未动判据编号 |
| N-6 | 「`:97` 同一路径重复 3 次」 | **已改**（非否决） | 见 1-9 落点映射行 |
| N-7 | 「`2-1` 或删去 Linux 降级表述、或由负责人先改最高设计」 | **取前者**（删降级表述） | `ARCHITECTURE.md:3` 自认「冲突时以最高设计为准」⇒ 默认按最高设计改本篇 |

**另需声明我自己的两处「未做」**：① 没有编译、没有运行任何二进制（判据全部来自源码阅读 + grep + schema 解析）；② `PERFORMANCE_MODEL.md` 我**没有改**（非我车道），但已在 `DATA_FLOW.md` 点明两式关系供改稿方对齐（见 §7）。

---

## 6. 自证段

### 6.1 我实际做了什么

- 逐行读完 5 份白名单主文档（`MODULE_MAP.md` 113 行、`ARCHITECTURE.md` 163 行、`DATA_FLOW.md` 186 行、`ABI.md` 117 行、`UNIFIED_OBJECTS.md` 147 行）+ `SECURE_LOADER.md` 118 行 + 审稿件 `T06-审稿-DOC-ENG.md` 457 行。
- 通读权威链相关段：`docs/ACSD_DESIGN.md` 第 7.2/7.3、8.2/8.3/8.5、第 9、10、11 章（全文 `##`/`###` 标题清单 dump 过一遍），`docs/science/unified/DATA_SEMANTICS.md` §3.8 精度表与 §5 判据，`docs/detail/UNIFIED_MODEL.md` 全文。
- **每条审稿判定都自己跑了命令复核**，没有只采信转述。复核中**推翻 1 条前提**（2-2 ABI 枚举缺项）、**降级 1 条**（2-3）、**收窄 1 条**（5-12）、**否决 1 条改法**（8-13 照字面删）。
- 手写死链扫描器跑了两轮：第一轮用审稿给的正则（漏了 CLI 内联与 doc 相对路径），第二轮自写并**修正了扩展名交替顺序 bug**（`c` 排在 `cpp` 前导致 `.cpp` 被截成 `.c` 产生 18 条假阳性），第三轮加「至少含一级目录」的约束并同时按 doc 相对 + 仓根相对两路解析，得到最终 0 条。

### 6.2 验证输出（逐条贴）

```
$ cd "/workspace/Astro CS Database"
$ W="docs/engineering/architecture docs/engineering/UNIFIED_OBJECTS.md docs/engineering/api/abi"

$ grep -rn "docs/detail/common" $W | wc -l
0                                                       ← 5-10 要求：0 ✓

$ grep -rn "eng/tests/contracts\|eng/tests/config" $W
docs/engineering/UNIFIED_OBJECTS.md:89:| 独立 Oracle + 负向 mutation 验证面 | `eng/tests/contracts/product_family/`（当前无执行器，见下注） | verification |
count=1                                                 ← 唯一残留是「明示无执行器」的登记，非可执行命令 ✓

$ grep -rnE "V1[0-9]|V2[0-9]|R-[0-9]+|P-[0-9]+|已删|旧版" $W | wc -l
5                                                       ← 全部为 schema $id 的 /v1 与 §4.1，假阳性 ✓

$ grep -n "acsd_head\|acsd_status\|acsd_handle" docs/engineering/api/abi/ABI.md
11:两者共享基础类型名（`acsd_head` / `acsd_status` / `acsd_span` 系列 / `acsd_allocator`）…
20:- 类型一律 `acsd_*` 小写前缀：`acsd_head`、`acsd_span_f32`…`acsd_status`…`acsd_handle`…
30:typedef struct { uint32_t struct_size, abi_version; } acsd_head;
33:typedef struct acsd_span_f32 { acsd_head head; … } acsd_span_f32;
34:typedef struct acsd_span_f64 { acsd_head head; … } acsd_span_f64;
35:typedef struct acsd_span_u8 { acsd_head head; … } acsd_span_u8;
41:typedef struct acsd_handle_s* acsd_handle;
50:} acsd_status;
85:`acsd_status` 是 ABI 层的粗粒度返回码…
104:| 类型、握手头字段、`acsd_status` 取值 | …`status_codes.h`…与 legacy 单头 `common_abi_v1.h`…
109:| 进程退出码与面向用户的失败分类 | …`acsd_status` 只做 ABI 层粗分类 |

$ ls lib/include/acsd/abi/ lib/include/acsd/abi/../
lib/include/acsd/abi/:   artifact_api_v1.h  host_api_v1.h  lifecycle_v1.h  module_api_v1.h  status_codes.h
lib/include/acsd/abi/../: abi  common_abi_v1.h  contracts  core  io  README.md   ← 两套头并存 ✓

$ python3 -c "import json;d=json.load(open('eng/contracts/schemas/unified/frame_snr.schema.json'));print(d['properties'].get('scalar'))"
None                    ← frame_snr 无 scalar 键；precision 枚举 ['float32','float64','integer'] ✓

$ python3 -c "import json;d=json.load(open('lib/infrastructure/pipeline/module_ports.registry.json'));print([m['module_id'] for m in d['modules']][:8])"
['acsd.phase1.calibration', 'acsd.phase1.cosmetic', 'acsd.phase1.wcs-platesolve',
 'acsd.phase1.star-psf', 'acsd.phase1.photometry', 'acsd.phase1.noise-snr',
 'acsd.phase1.drizzle', 'acsd.phase1.writer']                    ← 天体定位在星点 PSF 之前 ✓
```

**白名单死链终检（自写扫描器，doc 相对 + 仓根相对两路解析，要求路径含至少一级目录）**：

```
$ python3 - <<'PY'  … 见正文 6.1 第三节 …
## docs/engineering/architecture/MODULE_MAP.md MISSING: 5
    16 src/module_entry.c
    17 src/module_entry.cpp
    18 src/module_entry.cpp
    19 src/module_entry.cpp
    20 src/module_entry.cpp
TOTAL dead path refs (with >=1 dir component): 5
```

这 5 条**全部合法**：它们是表格「证据锚」列里相对同行「路径」列的简写。逐条实测：

```
$ for d in lib/infrastructure/gaia_xpsd_client lib/algorithms/drizzle \
           lib/algorithms/calibration lib/algorithms/cosmetic lib/algorithms/drizzle/hips; do
    ls "$d/src/module_entry."*; done
lib/infrastructure/gaia_xpsd_client/src/module_entry.c
lib/algorithms/drizzle/src/module_entry.cpp
lib/algorithms/calibration/src/module_entry.cpp
lib/algorithms/cosmetic/src/module_entry.cpp
lib/algorithms/drizzle/hips/src/module_entry.cpp
```

⇒ **白名单死链从改前 9 条（5 条仓根相对 + 4 条 CLI 内联）降到 0 条真死链。**

### 6.3 改动面自证

```
$ git -c core.quotepath=false diff --name-only -- \
    docs/engineering/architecture/ docs/engineering/api/abi/ docs/engineering/UNIFIED_OBJECTS.md
docs/engineering/UNIFIED_OBJECTS.md
docs/engineering/api/abi/ABI.md
docs/engineering/api/abi/SECURE_LOADER.md
docs/engineering/architecture/ARCHITECTURE.md
docs/engineering/architecture/DATA_FLOW.md
docs/engineering/architecture/MODULE_MAP.md

$ git -c core.quotepath=false diff --stat -- （同上 6 份）
 UNIFIED_OBJECTS.md  | 48 ++++++++++++++++-----------
 api/abi/ABI.md      | 46 +++++++++++++++----------
 api/abi/SECURE_LOADER.md | 10 +++---
 architecture/ARCHITECTURE.md |  6 ++--
 architecture/DATA_FLOW.md  | 16 ++++-----
 architecture/MODULE_MAP.md | 19 ++++++-----
 6 files changed, 84 insertions(+), 61 deletions(-)
```

**全部 6 份都在白名单内**；`contracts/`、`standards/`、`resources/`、`testing/`、`data/`、`governance/`、`build/`、`docs/engineering/README.md`、`docs/DOCUMENT_INDEX.yaml` **零改动**（工作树里那些改动是并发子代理的，不是我）。全程**无 git 写操作**，无 commit、无 add、无 stash。

### 6.4 诚实边界

- **没有编译、没有运行任何二进制**。所有「以代码为准」的判定来自源码阅读 + `grep` + JSON schema 解析。
- **没有取任何外部文献原文**。本轮未引用外部文献，故无此项。
- **对审稿 §1.1 列出的未读文档不作任何独立判断**。
- `run/GOVERN-08/审核包-R2/UNRESOLVED.md` 与仓内 `docs/engineering/governance/UNRESOLVED.md` 存在双编号空间、且后者不在我白名单，我只按 AGENTS §9「登记为 UNRESOLVED」在文档内指向 `governance/UNRESOLVED.md`，**没有写入任何条目**——写入需要治理面车道或负责人。

---

## 7. 给并行车道的三条对齐锚

1. **`resources/PERFORMANCE_MODEL.md`（改稿方）**：以 `architecture/DATA_FLOW.md` 为正本。原式 `I = max(1, L / F)` 只在 `n ≥ frame_workers` 时成立；`n < frame_workers` 时必须用 `min(n, frame_workers)`。DATA_FLOW 现已逐字写出这句，请照此补前提。
2. **`data/ARTIFACTS.md`（改稿方）**：本篇已把上位正本统一为 `docs/detail/UNIFIED_MODEL.md` 的「数据对象（各自具名）」一节。**该节既不含「13 个对象 → canonical schema → schema ID」，也不含「weight/value/scale/sigma/snr 歧义映射」**——后者是 ARTIFACTS.md `:98` 的本篇自有章节标题，被误挂到 UNIFIED_MODEL 头上，请一并订正。
3. **`governance/UNRESOLVED.md`（写入方）**：本轮产生 2 条待登记（U-7 精度归属、U-1 块载体）+ 1 条 MODULE_MAP 状态面（运行期宿主接线）。我只登记未写。

### 7.1 DATA_FLOW 式子逐字（供核对，我确认自洽且变量齐备）

```text
in_flight = min(n, frame_workers) // frame_workers = min(lease, 内存闸门上限)
inner_omp = max(1, thread_budget / in_flight) // thread_budget = lease
总并行度 = in_flight × inner_omp ≤ thread_budget
```

内存闸门上限：`cap = floor(MemAvailable × 0.75 / (W×H×B/px))`

有效宽度第三因子：`W_eff = in_flight × min(inner_omp, K)`

退化关系：当 `n ≥ frame_workers` 时 `in_flight = frame_workers`，上式等价于 `I = max(1, L / F)`，其中 `I = inner_omp`、`L = lease`、`F = frame_workers`；`n < frame_workers` 时必须回到 `min(n, frame_workers)` 的原式，**两条不可互换**。

不变式：帧级宽度未被内存压低时退化为 `inner_omp = 1`，只改变预算怎么用，不改变任何节点的数值路径。

**我未验证的**：`:125` 那句「因此 `K = num_threads` 是达成满宽的**唯一最小取值**」的**充分性**——推导只用到必要条件 `K ≥ inner_omp`，从它推不出 `num_threads = inner_omp`。这与审稿 6-6 对 `PERFORMANCE_MODEL.md` 的同一条指摘是同一问题。该句**不在我的派单范围**（3-5 只要求「式子自洽、变量都有定义」，`K`/`num_threads`/`W_eff` 我已补齐定义），我**保留原句未改**，在此登记请改稿方与评审一并处理。

---

## 8. 收敛判断

**本车道未达收敛。** 本轮处置 20 条编号（1-11、2-1、2-4、2-5、2-6、2-16、2-17、3-5、5-10、5-12、5-15、6-1、6-2、7-1、8-3、8-4、8-9、8-13、9-12、10-1、10-3），其中 **17 条已改、3 条登记待裁（U-7 / U-1 / U-3）**、**1 条推翻审稿前提（2-2）**、**4 条代码侧订正待排期（C-1..C-5）**。审稿十轮中属于本白名单的轮次仍有未消化项：`DATA_FLOW.md:125` 的充分性（§7.1）、`:121` 内存安全系数 `0.75` 与 `PERFORMANCE_MODEL.md` 的双份来源（4-1/4-2，非我车道）。