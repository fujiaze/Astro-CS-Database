# Module Map

> 上游：`docs/ACSD_DESIGN.md` §8（软件架构）、§12.5（状态阶梯）

状态词口径 = `docs/ACSD_DESIGN.md` §12.5（不另立阶梯）；本文件的表格**不写状态字段**——
各面状态由交付状态**现场计算**并落在
`docs/engineering/MODULE_MAP.md`，本文件只承载映射与可核证据锚。
每行按 `lib/` 实际目录登记，`docs/detail/registry/<module>.md` 为对应 L5 详细文档。

## 1 交付面（进构建 / 安装树）

| 模块 | 路径 | 产物 | 证据锚 |
| --- | --- | --- | --- |
| conformance (noop) | `lib/infrastructure/scheduler`（target 定义在根构建） | `modules/acsd_noop.so` | `eng/packaging/acsd.product.json` unit `MOD-NOOP` = `acsd.conformance.noop`；安全 loader 安装加载检查（`eng/tests/abi/mod001_install_load_check.py`）覆盖正路径与哈希 / module_id / 路径逃逸 / 文件缺失四类负路径 |
| catalog gaia | `lib/infrastructure/gaia_xpsd_client` | `modules/acsd_catalog_gaia.so` | unit `MOD-CAT-GAIA` = `acsd.catalog.gaia`；`src/module_entry.c` 九操作 + `acsd_module_query_v1`；selftest 装配 |
| p1 drizzle | `lib/algorithms/drizzle` | `modules/acsd_p1_drizzle.so` | unit `MOD-P1-DRIZZLE`；`src/module_entry.cpp`（C ABI v1 九操作）；装配 selftest |
| p1 calibration | `lib/algorithms/calibration` | `modules/acsd_p1_calibration.so` | unit `MOD-P1-CAL`；`src/module_entry.cpp`；装配 selftest |
| p1 cosmetic | `lib/algorithms/cosmetic` | `modules/acsd_p1_cosmetic.so` | unit `MOD-P1-COS`；`src/module_entry.cpp`；装配 selftest |
| p1 hips_writer | `lib/algorithms/drizzle/hips` | `modules/acsd_p1_hips_writer.so` | unit `MOD-P1-HIPSW`；`src/module_entry.cpp`；装配 selftest |
| cpu baseline provider | `lib/infrastructure/benchmark/backend_host` | `providers/acsd_cpu_baseline.so` | unit `PROV-CPU-BASELINE`；`baseline_backend.cpp` / `backend_loader.cpp` |
| CLI 平台单元 | `lib/infrastructure/cli/` | `acsd`（Windows `acsd.exe`） | unit `PLATFORM-CLI`；`parser.cpp` 的 `kRuleViews` 镜像命令树（`command_tree.h`）；判据 `eng/tests/cli/test_cli001_vpi.py` |
| runtime / io 平台单元 | `lib/infrastructure/scheduler`、`lib/infrastructure/aio/io` | `libacsd_runtime.so` / `libacsd_io.so`（Windows `acsd_runtime.dll` / `acsd_io.dll`） | units `PLATFORM-RUNTIME` / `PLATFORM-IO`；安装树契约见 `eng/packaging/install-tree.contract.json` |

> 安装面唯一源：`eng/cmake/install_layout.cmake`（五科学模块 SHARED + `$ORIGIN` RPATH）
> + `eng/packaging/acsd.product.json`（units）+ `eng/packaging/install-tree.contract.json`。
> 双平台产物名以实际构建产物为准（Linux `.so` / Windows `.dll`）。

### 1.1 C ABI 动态装载通道（未启用面声明）

本节声明**未启用面**（不是能力宣称），判据门 = `CHK-PROD-WIRING`
的 `W6 plugin_entry_unreachable` 与
`W1 declared_unreachable:manifest.entrypoint / integration.{op_entry,unique_entry}`。

- **声明事实**：`acsd_registry_open_v1`（`lib/infrastructure/pipeline/module_loader/module_registry.c`）
  在 `lib/**` 生产源零调用者；其唯一宿主解析点 `acsd_secure_loader_load_v1`
  （`secure_loader.c`，内含 `dlsym("acsd_module_query_v1")` 与
  `dlsym("acsd_provider_query_v1")`）不在三个生产命令（`cmd_session{1,2,3}_run`）
  的调用图上。⇒ 各模块 `module.yaml#entrypoint` 与
  `integration.json#dll.unique_entry` / `operations[].entry` 声明的九操作入口，
  **不由三个命令在运行期 dlopen**。
- **生产运行面的模块注册表是构建内的**：`acsd::ModuleRegistry`
  （`lib/include/acsd/core/module.h`）+ 节点适配器表
  （`lib/infrastructure/scheduler/src/module_adapters.cpp`，见 §2），已接线并经
  `p1001_real_nodes` / `p2001_real_nodes` / `p3002_real_nodes` 判据验证。
  §1 的五个科学模块 DLL 属**交付 / 安装面**，由安全 loader 探针逐 unit 装配验证。
- **消费者（仓内存在且走通）**：
  - `eng/tests/abi/mod001_install_load_check.py` —— 安全 loader 逐 unit 加载 5 个科学模块
    DLL + noop，正路径（sha256 / module_id / allowed_root 三校验）与 4 类负路径
    （HASH_MISMATCH / MODULE_ID_MISMATCH / PATH_ESCAPE / FILE_MISSING）必败；
  - `eng/tests/abi/test_module_registry.py` 与 `eng/tests/abi/test_abi005_echo.py`
    （module 合同与三方一致正测）；
  - 安装树产品清单核对。
- **缺口登记**：运行期宿主接线未落地一项登记于 `docs/KNOWN_LIMITATIONS.md`，
  其收敛按 `docs/engineering/MODULE_MAP.md` 与 `CHK-PROD-WIRING` 的现场计算结论判定；
  收缩声明面须经 `docs/detail/registry/**` 变更单按最高设计 §8.1 走变更流程。
- 相邻事实：`eng/packaging/acsd.product.json` 的 note 自述 PLATFORM-RUNTIME / IO
  仍未落实现；`lib/infrastructure/pipeline/module_loader/README.md` 自述 host(registry)
  接线属平台运行时工单。

## 2 会话与节点执行面（构建内，非独立 DLL）

| 模块 | 路径 | 职责 | 证据锚 |
| --- | --- | --- | --- |
| Runtime / 唯一 executor | `lib/infrastructure/scheduler` | typed DAG 调度、ThreadBudget 租约、进程唯一 worker 池 | `lib/infrastructure/scheduler/src/executor_runtime.h`、`lib/infrastructure/scheduler/src/module_adapters.cpp`；ctest `rt001_unique_executor` |
| 模块注册表（三 Phase 节点） | `lib/infrastructure/scheduler` | P1 八节点（calibration / cosmetic / drizzle / noise-snr / photometry / star-psf / wcs-platesolve / writer）、P2 七节点（coverage / sample / upm-fit / upm-apply / reject / integrate / write）、P3 五节点（properties / wcs / resample / verify / writer），各绑唯一真实 operation（`docs/ACSD_DESIGN.md` §4.2 / §5.2 / §6.2） | `lib/infrastructure/scheduler/src/module_adapters.cpp`；ctest `p1001_real_nodes` / `p2001_real_nodes` / `p3002_real_nodes` / `p3002_uncertainty` |
| Phase1 会话 | `lib/phase1_session` | `io_read → calibrate → cosmetic → io_write` | `lib/phase1_session/p1_session.cpp`（`manifest["stages"]`）；unit `entrypoint: p1_session_run` |
| Phase1 科学内核 | `lib/algorithms/photometry`、`lib/algorithms/star_detection`、`lib/algorithms/psf`、`lib/algorithms/platesolve`、`lib/algorithms/calibration`、`lib/algorithms/cosmetic` | 校准、检测 / PSF、天文定位、测光定标、噪声模型 | 各目录 `src/` 内真实源文件（逐内核 operation 见 `module_adapters.cpp`） |
| Phase2 会话 | `lib/phase2_session` | 七节点链组装（coverage → sample → upm-fit → upm-apply → reject → integrate → write） | `lib/phase2_session/p2_session.cpp`；ctest `p2001_real_nodes` |
| Phase2 内核 | `lib/algorithms/coverage` | `lib/algorithms/coverage/src` 下 coverage / sampler / upm / rejection / integrate / stage2_common 源文件 | 同上 + `eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json` |
| Phase3 会话 | `lib/phase3_session` | properties / WCS（TAN）/ nearest+bilinear 重采样 / CFITSIO 原子写 / verify | `lib/phase3_session` 的 p3_session / p3_wcs / p3_resample / p3_output 四源文件；ctest `p3002_real_nodes` / `p3002_uncertainty` |
| 三阶段产品交换 | `lib/infrastructure/aio/runtime/artifact_store` | 跨 Phase 仅磁盘产品交换（role ↔ type 强绑定） | `eng/contracts/data/phase_product_exchange.schema.json` + `lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py` |
| 结构化日志 | `lib/infrastructure/observability/logging` | JSONL 事件合同 | `docs/engineering/observability/STRUCTURED_LOGGING_CONTRACT.md` + 其 schema |
| 监控与资源门 | `eng/tools/monitoring` | 冻结阈值判定（`docs/ACSD_DESIGN.md` §8 + `eng/contracts/resource_gate_v1.json`） | `eng/tools/monitoring/run_monitored.py` 的 `evaluate_frozen_gate()`；pytest `eng/tests/monitoring` |
| AIO 图像 I/O | `lib/infrastructure/aio` | FITS / XISF / HiPS 读写、唯一 AIO C ABI v1 | `lib/infrastructure/aio/src/aio_abi.cpp`（编入生产 target `acsd_aio`） |
| HEALPix / Drizzle 内核 | 生产实现 = `lib/algorithms/drizzle/healpix_drizzle`；归档面 = `lib/infrastructure/aio/healpix_db/archive`（`healpix_io` 与 `healpix_browser_qt` 不重建） | HEALPix 球面重采样、drizzle 累加与归并 | `lib/algorithms/drizzle/healpix_drizzle`；归档目录 `lib/infrastructure/aio/healpix_db/archive` |
| 公共工具 | `lib/algorithms/shared` | HEALPix core / SHA-256 / compute traits（header-only + 静态） | `lib/algorithms/shared` |

## 3 合同 / 迁移目标目录（尚无独立 DLL 产物）

| 迁移目标 | 路径 | 现状与去向 |
| --- | --- | --- |
| p3 projection | `lib/algorithms/projection` | 设计冻结 8 种投影（`TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA`，最高设计 §6.3）；**在役 registry = `p3_proj.h`/`p3_proj.cpp`**，可作产品声明的投影以 `p3_projection_registry.h` 的当前声明为权威，未实现项报「不支持」；DLL 挂载与生产会话切换归迁移目标 P3 投影接入 |
| p3 resample | `lib/algorithms/resample` | 目标产物 `acsd_p3_resample`；生产实现在 `lib/algorithms/resample/p3_resample.cpp`；顶层占位 descriptor `acsd.phase3.resample` 接入归迁移目标 P3 重采样接入 |
| p3 fits | `lib/algorithms/fits_output` | 目标产物 `acsd_p3_fits`；生产实现在 `lib/algorithms/fits_output/p3_output.cpp`；流式 FITS 接入未实现 |
| phase2 upm / samp / rej / int | `lib/algorithms/upm`、`lib/algorithms/sampling`、`lib/algorithms/rejection`、`lib/algorithms/integration` | 生产实现在 `lib/algorithms/coverage`（节点化已在役）；独立 DLL 化为迁移目标 |
| hips_p2 | `lib/algorithms/coverage/hips_p2` | Phase2 HiPS 写出目标；生产路径在 `lib/infrastructure/aio` + `lib/algorithms/coverage` |
| plate_solve / photometric_calib / star_detector / dynamic_psf | `lib/algorithms/platesolve`、`lib/algorithms/photometry`、`lib/algorithms/star_detection`、`lib/algorithms/psf` | 已作为 Phase1 节点唯一真实 operation 接入（`module_adapters.cpp`）；独立 DLL 化未做 |

## 4 非交付面

| 目录 | 说明 |
| --- | --- |
| `lib/infrastructure/acr` | 异构计算抽象；根 `CMakeLists.txt` 默认 OFF，生产 target 不链；保留源码与隔离测试（最高设计 §1.4） |
| `lib/infrastructure/pipeline/orchestrator` | Phase1 编排已并入 CLI pipeline driver，无独立 exe；非生产入口 |
| `lib/infrastructure/hips_browser/healpix_browser_qt` | HiPS 浏览器（optional，不入 product manifest）；工具分类（非发布） |
| `lib/algorithms/noise_snr` | 不在根构建图（未 `add_subdirectory`）；`eng/tests/unit` 门卫为 `if(EXISTS)` / `if(TARGET)` |
| `aio_pipeline_engine` 越权编排 | `lib/infrastructure/aio/src/aio_pipeline_engine.cpp` 在位但不由生产命令驱动 |

## 5 每模块详细文档

`docs/detail/registry/<module>.md`（L5 模板）；模块清单以本表 §1–§4 为准。

## 6 文档符号命名空间登记

文档正文反引号里的 token 必须能解析到**已登记的命名空间之一**，否则无法判定它指什么。
本节登记该判据的分类轴与逐条归属：token 本身在正文与源码中大量出现，**归属不可由 token 反推**，
故在此逐条落盘。

**自动命名空间**（无需逐条登记，按面自动解析）：

| 命名空间 | 解析面与口径 |
|---|---|
| `document_stems` | docs/**/*.md — docs/**/*.md 的词干（含 _ 边界后缀匹配，用于 `SCI-ADJ-001_CONFLICT_MATRIX.md` 的简写形式 CONFLICT_MATRIX） |
| `glossary_terms` | docs/GLOSSARY.md — docs/GLOSSARY.md = 唯一术语权威（冻结定义，18 术语，术语权威校验项） |
| `public_headers` | lib/**/*.h, lib/include/**/*.h — lib/**、lib/include/** 公开头中真实定义的枚举常量/宏（公开 API 面只登记函数签名，见 `docs/engineering/PUBLIC_API.md`） |

**逐条登记**（39 条）：

| # | token | 命名空间 | 证据锚 | 归属理由 |
|---:|---|---|---|---|
| 1 | `snr_v` | `science_quantity` | `docs/GLOSSARY.md:15` | SCI-CW 域科学量（帧质量权重 = support×snr_v²），术语权威 docs/GLOSSARY.md:15（frame_quality_weight 行）【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 2 | `p2_write_descriptor` | `registry_descriptor` | `lib/infrastructure/scheduler/src/module_adapters.cpp:1192` | Phase2 write 节点的 registry descriptor 构造器（编排层词汇；api_inventory 只登记函数签名 ⇒ 需显式登记命名空间）。行锚 = 定义行 :1173（`ModuleDescriptor p2_write_descriptor() {`；调用点 :16176 p2_nodes[] 表）。原锚 :1166 逐字是 `d.alg_id = "ALG-P2-INT-001";`，落在**上一个函数** p2_integrate_descriptor（定义行 :1153）体内，不含本 token ⇒ 逐条核验订正（C-35 同型行锚漂移）【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 3 | `p2_parallel_for` | `orchestration_helper` | `lib/infrastructure/scheduler/src/module_adapters.cpp:9258` | Phase2 tile 级并行原语（翻译单元内 static，非公开 ABI ⇒ 不进 api_inventory）。行锚 = 定义行 :9236（`static void p2_parallel_for(uint32_t workers, uint64_t n, Fn&& body) {`）；调用点 :10618（p2_op_upm_apply，定义行 :10444）/ :11224（p2_op_reject，定义行 :11068）/ :12396（p2_op_integrate，定义行 :11878）。原锚 :9068 逐字是 `{"variance_census_available", variance_census_available},`，落在另一个函数 p1_op_writer（定义行 :8659）体内，不含本 token ⇒ 逐条核验订正【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 4 | `snr_path_effective` | `doc_header_kv` | `docs/detail/PHASE2_DETAILED_DESIGN.md:21` | SNR 重建口径的实际生效路径记录名（头部 KV），非代码符号；现行承载见 docs/engineering/PIPELINE_BLOCK_CONTRACT.md:57 与 docs/detail/registry/acsd.phase1.noise-snr.md「SNR 三条路径与稀疏帧内层」（`snr_path_effective` 登记行）（原写 PIPELINE_BLOCK_CONTRACT.md:58，该行逐字不含本 token，核验订正为 :57；evidence 行 :21 逐字命中，锚未动）【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 5 | `P1.HIPS_WRITE` | `stage_id` | `docs/engineering/ERROR_HANDLING_STANDARD.md:67` | 阶段 ID（normalize 阶段的 HiPS 写出节点），节点名与 docs/modules/registry/ 注册表一致；阶段 ID 面唯一清单 = docs/engineering/ERROR_HANDLING_STANDARD.md「阶段 ID」节【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 6 | `P2.HIPS_WRITE` | `stage_id` | `docs/engineering/ERROR_HANDLING_STANDARD.md:68` | 阶段 ID（mosaic 阶段的 HiPS 写出节点），节点名与 docs/modules/registry/ 注册表一致；阶段 ID 面唯一清单 = docs/engineering/ERROR_HANDLING_STANDARD.md「阶段 ID」节【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 7 | `COUNT_FIELD_MISSING` | `verdict_id` | `docs/engineering/DATA_ARTIFACTS.md:37` | DATA-P3-REJ-001 的判据名（产品缺 diagnostic_planes.n_rejected_nonfinite 时判红；「0 与字段缺失必须可区分」，DATA-002 §2a 规则 3）。非 API 符号 ⇒ 显式登记命名空间。判据。文档承载 = docs/engineering/DATA_ARTIFACTS.md DATA-P3-REJ-001 行。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 8 | `PHOT_PASSBAND_IDENTITY_INCONSISTENT` | `verdict_id` | `lib/infrastructure/scheduler/src/module_adapters.cpp:6046` | 组内模型通带唯一性的帧级失败码（某帧解析出的通带与组内首帧不一致 ⇒ 该帧判此码；帧间独立，不阻塞其他帧）。全仓唯一产生点 = module_adapters.cpp:6024 的 mark_frame_fail 调用（帧间通带比较的 else-if 分支体内），同函数 p1_op_photometry 定义行 :5309。原锚 :5856 逐字是 `if (!sip_ok) {`，虽同在 p1_op_photometry 体内但属**另一判据分支**（→ PHOT_WCS_SIP_INVALID），不含本 token ⇒ 逐条核验改到逐字命中行【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 9 | `FROZEN_SCI_CLAIM` | `anchor_contract_kind` | `eng/contracts/anchors/anchor_contract.json:188` | 锚合同 exemptions 的豁免取值（ALG 文档引述 SCI 冻结声明原文，原文行号漂移属 SCI 侧已知，修改方向 = SCI→ALG）；文档承载 = ANCHOR_CONTRACT.md §4.1。行锚 = exemptions 内首条 kind 行 :188（`"kind": "FROZEN_SCI_CLAIM",`）。原锚 :176 逐字是 `},`，落在上一数组 bindings 末条记录的收尾行 ⇒ 逐条核验订正【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 10 | `EXTERNAL_REFERENCE` | `anchor_contract_kind` | `eng/contracts/anchors/anchor_contract.json:212` | 外部引用取值（未 vendor 进仓的外部开源实现 / 外部许可证据），两个登记面各自成条：anchor_contract.json.exemptions（本条 evidence）与 unresolved_registry.json.entries；文档承载 = ANCHOR_CONTRACT.md §4.1。行锚 = exemptions 内该 kind 行 :212（`"kind": "EXTERNAL_REFERENCE",`）。原锚 :216 逐字是 `}`，是同一条记录**下一行**的收尾 ⇒ 逐条核验订正【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 11 | `HISTORICAL_NARRATION` | `anchor_contract_kind` | `eng/contracts/anchors/unresolved_registry.json:23` | 未解析登记台账的取值：正文订正叙述里被引的订正前旧锚（被引文件不在本仓），随该叙述段删除而同提交删除登记项；文档承载 = ANCHOR_CONTRACT.md §4.1。行锚 = entries 内 该 kind 行 :23（`"kind": "HISTORICAL_NARRATION",`）。原锚 :15 逐字是 `"kind": "EXTERNAL_REFERENCE",`，落在 entries 首条（另一 kind）记录内 ⇒ 逐条核验订正【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 12 | `ACSD_BACKENDS_MANIFEST` | `cmake_variable` | `CMakeLists.txt:941` | backend 族清单（backends.manifest.json）的 CMake 变量，生成与安装两处消费（eng/cmake/install_layout.cmake:115）；文档承载 = docs/engineering/ISA_VARIANTS.md §2.2 表「构建产物名」列。行锚 = set() 定义行 :941（`set(ACSD_BACKENDS_MANIFEST "${ACSD_ISA_PROVIDER_DIR}/backends.manifest.json")`）。原锚 :811 逐字是 `#`，落在变体交付形态的注释块内 ⇒ 逐条核验订正【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 13 | `ACSD_PROVIDERS_MANIFEST` | `cmake_variable` | `CMakeLists.txt:1052` | provider 族清单（providers.manifest.json）的 CMake 变量，生成与安装两处消费（eng/cmake/install_layout.cmake:135）；文档承载 = docs/engineering/ISA_VARIANTS.md §2.2 表「构建产物名」列。行锚 = set() 定义行 :1052（`set(ACSD_PROVIDERS_MANIFEST "${ACSD_ISA_PROVIDER_DIR}/providers.manifest.json")`）。原锚 :881 逐字是 `target_link_libraries(acsd_cpu_avx512_kernels PRIVATE acsd_contracts acsd_common)`，落在另一个目标体内 ⇒ 逐条核验订正【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 14 | `CONTRACT_READY` | `delivery_status` | `docs/engineering/RELEASE_STATUS.md:15` | 模块/交付物状态阶梯取值（ACSD_DESIGN §12.5 词表；文档映射表现场计算 status ∈ status_vocabulary，不在词表内即判红）。取值由模块映射表现场计算，不是公开 API 符号 ⇒ api_inventory（只登记函数签名）与 lib 公开头都不收。判据。文档承载 = docs/engineering/RELEASE_STATUS.md §0。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 15 | `NOT_IMPLEMENTED` | `delivery_status` | `docs/engineering/RELEASE_STATUS.md:19` | 模块/交付物状态阶梯取值（同 §12.5 词表；释义「能力不在当前基线（符号/路径不存在）」见 docs/engineering/RELEASE_STATUS.md §0）。非 API 符号 ⇒ 需显式登记命名空间。判据。文档承载 = docs/engineering/RELEASE_STATUS.md §0。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 16 | `NOT_VERIFIED` | `delivery_status` | `docs/engineering/RELEASE_STATUS.md:20` | 模块/交付物状态阶梯取值（同 §12.5 词表；释义「能力可能存在但当前提交未复跑执行验收」见 docs/engineering/RELEASE_STATUS.md §0）。两面的封闭词表都不在 api_inventory 与 lib 公开头 ⇒ 显式登记命名空间。文档承载 = docs/engineering/RELEASE_STATUS.md §0 与 §4。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 17 | `NOT_ATTEMPTED` | `verdict_id` | `docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:487` | 验收 tier 判词的封闭词表取值（判据口径：0 帧被求值且 not_attempted_reasons 非空）。判词不是 API 符号 ⇒ 显式登记命名空间。判据。文档承载 = docs/engineering/VALIDATION_EVIDENCE_STANDARD.md 帧级判词表。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 18 | `NOT_EVALUATED` | `verdict_id` | `docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:473` | 帧级判词三值之一（ACCEPT / REJECT / NOT_EVALUATED）。判词不是 API 符号 ⇒ 显式登记命名空间。文档承载 = docs/engineering/VALIDATION_EVIDENCE_STANDARD.md 帧级判词表（与 counts.not_attempted 对账）。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 19 | `FAULT_INJECT_NOOP` | `verdict_id` | `docs/engineering/STANDARDS_REGISTRY.md:341` | 注入空转守卫的失败判词：注入锚被确定性替换命中次数 ≠ 1 ⇒ 抛 InjectedNoOp 并 exit 3。产出者既不在 lib 公开头语料也不在 api_inventory ⇒ 显式登记命名空间。判据。文档承载 = docs/engineering/STANDARDS_REGISTRY.md 注入协议节。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 20 | `TRACEABILITY_MATRIX_PASS` | `verdict_id` | **已退役**（追溯面不设机器检查器，判词随之退役） | 追溯矩阵检查 exit 0 的成功判词（打印面；判据 C1–C7 见 docs/engineering/TRACEABILITY_SPEC.md §7）。判词不是 API 符号 ⇒ 显式登记命名空间。判据。文档承载 = docs/engineering/TRACEABILITY_SPEC.md 检查器节。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 21 | `EMPTY_CELL_VIOLATION` | `verdict_id` | `docs/engineering/TRACEABILITY_SPEC.md:143` | 追溯矩阵 C3 判据名（任何单元格为空/纯空白/'-'/'?'/'TBD'/'TODO' ⇒ FAIL 并报具体行/列）。判据名不是 API 符号 ⇒ 显式登记命名空间。文档承载 = docs/engineering/TRACEABILITY_SPEC.md。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 22 | `ABOVE_CEILING` | `verdict_id` | `实验/photometric-magnitude/code/scia_common.py:597` | 测光误差预算门的 verdict 取值（sigma_obs > sigma_ceiling ⇒ 翻为该判词；判定序全文见 实验/photometric-magnitude/README.md:83，读数面 = 同文件 §4.7 与该单元 `REPORT_experiment.md` / `REPORT_paper.md` 的真实帧腿段，σ_obs = 0.026520 > σ_ceiling = 0.020561）。判词由实验件产出，不在 lib 公开头语料也不在 api_inventory ⇒ 显式登记命名空间。文档承载 = docs/engineering/P1_SIGMA_GATE_PROVENANCE.md【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 23 | `ANCHOR_STALE` | `verdict_id` | `docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:408` | 「锚存活」失效的具名失败判词（判据 = 判据中硬编码引用的仓库路径必须存在；失效 ⇒ stderr 打印 ANCHOR_STALE 并 exit 2，不 traceback、不静默降级）。判词不是 API 符号 ⇒ 显式登记命名空间。行锚 = 判据条文行 :408。文档承载 = docs/engineering/VALIDATION_EVIDENCE_STANDARD.md 锚存活条与 docs/engineering/STANDARDS_REGISTRY.md §5【证据形态 C：仓内无该判词的代码抛出点载体，仅判据条文逐字承载 token；条文改写本门不判红 —— 已知代价，如实标注】 |
| 24 | `SOURCE_DIGEST_MISMATCH` | `verdict_id` | `docs/engineering/VERSIONING.md:45` | 构建溯源判据 N1 的判红名（记录 build_source_digest ≠ 按当前工作树重算；有构建树逐文件清单时精确点名差异文件）。判词不是 API 符号 ⇒ 显式登记命名空间。行锚 = 判据条文行 :45。文档承载 = docs/engineering/VERSIONING.md §2.1 构建指纹合同【证据形态 C：仓内无该判词的代码抛出点载体，仅判据条文逐字承载 token；条文改写本门不判红 —— 已知代价，如实标注】 |
| 25 | `BUILD_STAMP_ANCHOR_MISSING` | `verdict_id` | `docs/engineering/VERSIONING.md:46` | 构建溯源判据 N4 的判红名（目标里没有构建期指纹；fail-closed 语义「不可锚定 ≠ 通过」）。判词不是 API 符号 ⇒ 显式登记命名空间。行锚 = 判据条文行 :46。文档承载 = docs/engineering/VERSIONING.md §2.1 构建指纹合同【证据形态 C：仓内无该判词的代码抛出点载体，仅判据条文逐字承载 token；条文改写本门不判红 —— 已知代价，如实标注】 |
| 26 | `NON_CONFORMANT` | `standards_conformance_status` | `docs/engineering/STANDARDS_REGISTRY.md:26` | 「符合状态」轴（与外部标准条款的关系，取值域由四值封闭，与交付状态阶梯是**两个独立轴**，docs/engineering/RELEASE_STATUS.md §0 已声明两轴独立）的取值之一。取值域判据不在 lib 公开头语料也不在 api_inventory ⇒ 显式登记命名空间。判据。文档承载 = docs/engineering/RELEASE_STATUS.md §0 与 docs/engineering/STANDARDS_REGISTRY.md 符合状态列。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 27 | `PROJECT_DEFINED` | `standards_conformance_status` | `docs/engineering/STANDARDS_REGISTRY.md:27` | 「符合状态」轴的取值之一（标准未规定或本实现显式偏离标准之处，按 Project-defined 冻结；取值域同 NON_CONFORMANT 所述封闭词表）。与交付状态阶梯是**两个独立轴**。文档承载 = docs/engineering/RELEASE_STATUS.md §0 与 docs/engineering/STANDARDS_REGISTRY.md 符合状态列。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 28 | `REQUIRED_ANCHORS` | `checker_constant` | `docs/engineering/STANDARDS_REGISTRY.md:334` | STD-REG 门禁的必需锚清单常量（REGISTRY_REL / INDEX_REL，fail-closed：文件不存在或未跟踪 ⇒ ANCHOR_STALE + exit 2）。产出者既不在 lib 公开头语料也不在 api_inventory ⇒ 显式登记命名空间。判据。文档承载 = docs/engineering/STANDARDS_REGISTRY.md 锚存活节。【证据形态 C：仓内无该判词的代码抛出点载体（产出该判词的检查器已物理删除），仅判据条文逐字承载 token；条文被改写时本门不判红 —— 已知代价，如实标注】 |
| 29 | `ACSD_ENABLE_ACR` | `cmake_variable` | `CMakeLists.txt:18` | 根 CMake option（构建 dormant ACR 树，默认 OFF；状态回显 :1580，preset 面 CMakePresets.json:49/:76，正式路径必须 OFF 由 工具链锁定判据（正式路径必须 OFF 的断言） 断言）。CMake 变量不是 API 符号 ⇒ 显式登记命名空间。行锚 = option() 定义行 :18。文档承载 = docs/engineering/ARCHITECTURE_OVERVIEW.md 构建面表【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 30 | `p2_op_coverage` | `module_port_symbol` | `lib/infrastructure/scheduler/src/module_adapters.cpp:9389` | Phase2 coverage 模块的编排层端口符号（定义行 :9367；派发点 :14088；端口登记面 lib/infrastructure/pipeline/module_ports.registry.json:940/:961；模块→符号映射 端口↔代码双向一致判据）。与既有 p2_write_descriptor / p2_parallel_for 同型：编排层词汇，不进 api_inventory（只登记函数签名），也不在 lib 公开头。行锚 = 定义行 :9367。文档承载 = docs/detail/registry/acsd.phase2.coverage.md「公共 header、核心 symbol 与生命周期」（`p2_op_coverage` 登记行）【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 31 | `p2_op_reject` | `module_port_symbol` | `lib/infrastructure/scheduler/src/module_adapters.cpp:11105` | Phase2 rejection 模块的编排层端口符号（定义行 :11068；台账面 eng/contracts/ledgers/dead_config_keys.json 以该符号名记退出条件）。与既有 p2_write_descriptor / p2_parallel_for 同型：编排层词汇，不进 api_inventory，也不在 lib 公开头。行锚 = 定义行 :11068。文档承载 = docs/detail/registry/acsd.phase2.reject.md「公共 header、核心 symbol 与生命周期」（`p2_op_reject` 登记行）【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 32 | `sdet_gauss_fit` | `module_internal_symbol` | `lib/algorithms/star_detection/src/sdet_api.cpp:779` | 星检测模块翻译单元内的椭圆高斯拟合函数（模板 sdet_gauss_fit<T>，定义行 :779；调用点 :2076 与 :2331；模块登记面 lib/algorithms/star_detection/module.yaml:103）。不经导出宏、**不在 lib 公开头语料**（门内 _defined_in_public_header 实测判 False）也不在 api_inventory ⇒ 显式登记命名空间。行锚 = 定义行 :779。文档承载 = docs/detail/registry/acsd.phase1.star-detection.md「数值落地口径与实现落点」（两条实现路径表）【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 33 | `P2_SEMANTIC_` | `prefix_family_token` | `lib/algorithms/coverage/include/astro/phase2/rejection.h:66` | **前缀族 token 形态**（以 _ 结尾的族名）：族成员是同头 :66-78 的 12 条 #define（P2_SEMANTIC_NONE / _ROBUST_MAD_CLIP / _WINSORIZED_SIRIL / …）。门的 lib 公开头判据是词界匹配（词的 lib 公开头判据是词界匹配（`\b+token+\b`）），P2_SEMANTIC_ 后接字母时不存在词界 ⇒ 族名形态解析不到族内任一具体成员，而族名本身也不是 API 符号 ⇒ 必须显式登记该形态，否则文档写「前缀族」这一合法形态即被判红。行锚 = 族内首条 #define 行 :66。文档承载 = docs/detail/anchors/ANCHOR_CONTRACT.md L3 三形态（本形/前缀族/形态族）判别力用例【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 34 | `__AVX512CD__` | `platform_or_toolchain_constant` | `lib/infrastructure/benchmark/backend_host/avx512_backend.cpp:27` | 编译器预定义宏（AVX-512 CD 子集旗标），由编译器提供、不是本仓 API 符号；本仓消费面 = avx512_backend.cpp 的 DSO 自陈声明平台分支（本行），守卫面 lib/infrastructure/benchmark/cpu/avx512/src/avx512_kernels.cpp:26，测试侧强制保留该分支 eng/tests/backend/test_manifest_isa_declaration.py:40-41。⇒ 显式登记命名空间。行锚 = 平台分支行 :27。文档承载 = docs/engineering/ISA_VARIANTS.md 声明面节【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 35 | `__AVX512F__` | `platform_or_toolchain_constant` | `lib/infrastructure/benchmark/backend_host/avx512_backend_kernels.cpp:17` | 编译器预定义宏（AVX-512 F 子集旗标），由编译器提供、不是本仓 API 符号；本仓消费面 = 计算面 TU 顶部的许可面实测守卫（本行，_MSC_VER 且未定义该宏即 #error，见同文件 :16/:18；另一处 lib/infrastructure/benchmark/cpu/avx512/src/avx512_kernels.cpp:19）。⇒ 显式登记命名空间。行锚 = 守卫行 :17。文档承载 = docs/engineering/ISA_VARIANTS.md 旗标失效不得静默节【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 36 | `CTRL_CLOSE_EVENT` | `platform_or_toolchain_constant` | `eng/tests/cli/test_fix406_sigterm_cancel.py:28` | Win32 控制台事件类型常量（SetConsoleCtrlHandler 家族），由 Windows 平台提供、不是本仓 API 符号；仓内唯一承载面 = CHK-FIX406-SIGTERM 的平台限制登记（本行及 :27-30 说明 Python 无法程序化投递该事件）。⇒ 显式登记命名空间。文档承载 = CHK-FIX406-SIGTERM 的平台限制登记（本行）【证据形态 C：仓内无该平台常量的任何定义，evidence 行是测试文件里的平台限制说明行（逐字含 token）；门只判文件级，平台/上游变动不判红 —— 已知代价，如实标注】 |
| 37 | `RICE_1` | `external_upstream_symbol` | `lib/infrastructure/aio/third_party/cfitsio/fitsio.h:296` | FITS 标准 tile-compression 算法码（CFITSIO 宏，随 CFITSIO vendored 进仓；同族 GZIP_1 :297、PLIO_1 :299）。本仓消费面：读取路径接 CFITSIO 的 .fz 自动检测/解压（lib/infrastructure/aio/src/aio_fits.cpp:552）。**门的 lib 公开头语料显式排除 third_party**（词的 lib 公开头语料显式排除 third_party）⇒ 该宏不在 public_headers 命名空间，也不是本仓 API 符号 ⇒ 显式登记命名空间。行锚 = #define 行 :296。文档承载 = docs/science/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md 与 docs/engineering/COMPRESSION_CODEC_RESEARCH_PACK.md【证据形态 A：该行逐字含 token —— 行锚即为 token 的真实承载行】 |
| 38 | `COADD_WEIGHTED` | `external_upstream_symbol` | `docs/engineering/SCIENTIFIC_REFERENCES.md:183` | 上游 SWarp 的 coaddtype 枚举取值（逐帧背景 RMS 的逆方差组合，输出方差 `1/Σ(1/var_k)`，上游锚 `src/coadd.c:1279-1311`）。SWarp 未 vendor 进仓。不是本仓 API 符号 ⇒ 显式登记命名空间（与既有 EXTERNAL_REFERENCE 登记同型）。文档承载 = docs/engineering/SCIENTIFIC_REFERENCES.md SWarp 行（该行把同一符号钉到版本 2.41.5 = 5c927e8a9312576b8618bf2480be1b9867d2483c）。**如实标注本条的证据强度**：仓内逐字保存上游源码片段的载体随实验域运行结果归档一并移除，本仓现无代码或产物载体逐字承载该 token，证据只落在钉住上游 commit 与文件行的文档承载面（另两处文档复述为 实验/absolute-snr/docs/SNR_WEIGHT_RESEARCH_PACK.md:198）；代价是若上游删除该取值，本门不会判红【证据形态 C：仓内无代码/产物载体，仅文档散文逐字承载 token —— 已知代价，如实标注】 |
| 39 | `RESCALE_WEIGHTS` | `external_upstream_symbol` | `docs/engineering/SCIENTIFIC_REFERENCES.md:183` | 上游 SWarp 的 coaddtype 枚举取值（实测噪声重标定 sigfac）。SWarp 未 vendor 进仓。**如实标注本条的证据强度**：该 token 在版本库面只出现在文档里，无代码或产物载体（另两处文档复述为 docs/science/NOISE_MODEL.md:394 与 实验/absolute-snr/docs/SNR_WEIGHT_RESEARCH_PACK.md:52），故 evidence 落在钉住上游 commit 与文件行的文档承载面上，与既有 EXTERNAL_REFERENCE 登记同型；代价是若上游删除该取值，本门**不会**判红。文档承载 = docs/engineering/SCIENTIFIC_REFERENCES.md SWarp 行【证据形态 C：仓内无代码/产物载体，仅文档散文逐字承载 token；门只判 evidence 文件存在且含 token，上游删该取值本门不判红 —— 已知代价，如实标注】 |

上表 6 条证据锚在本次登记时已随 `docs/architecture/`、`docs/algorithms/anchors/`、
`docs/modules/` 的删除而换址，已按现行正本重定位；重定位只改落点，不改归属。
