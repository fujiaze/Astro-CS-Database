# ACSD 模块地图

本文依据最高设计[1]的软件架构、产品与发行边界，登记模块到目录、产物和实现位置的映射。组件边界、构建闭包和端口分别由系统架构、构建图及命名块合同说明[2–4]。

本表登记模块、目录、声明产物和定位入口，区分安装面与真实命令调用面。产物名、descriptor 和测试名称不证明构建、运行或科学验证；具体执行证据按追溯规范[5]报告。对应模块正文按阶段集中在 detail/normalize、detail/mosaic、detail/export，其接口与状态需要和真实公开头及调用图核对。

## 交付面（进构建 / 安装树）

| 模块 | 路径 | 产物 | 证据锚 |
| --- | --- | --- | --- |
| conformance (noop) | `eng/tests/conformance/noop`（target `acsd_noop` 在该子目录声明，由根构建图 `add_subdirectory` 引入） | `modules/acsd_noop.so` | `eng/packaging/acsd.product.json` 单元 `MOD-NOOP` = `acsd.conformance.noop`；安全装载器的安装加载覆盖正路径（哈希、模块标识、授权根）与路径逃逸、文件缺失、模块标识不符、哈希不符四类负路径 |
| catalog gaia | `lib/infrastructure/gaia_xpsd_client` | `modules/acsd_catalog_gaia.so` | unit `MOD-CAT-GAIA` = `acsd.catalog.gaia`；`src/module_entry.c` 九操作 + `acsd_module_query_v1`；selftest 装配 |
| p1 drizzle | `lib/algorithms/drizzle` | `modules/acsd_p1_drizzle.so` | unit `MOD-P1-DRIZZLE`；`src/module_entry.cpp`（C ABI v1 九操作）；装配 selftest |
| p1 calibration | `lib/algorithms/calibration` | `modules/acsd_p1_calibration.so` | unit `MOD-P1-CAL`；`src/module_entry.cpp`；装配 selftest |
| p1 cosmetic | `lib/algorithms/cosmetic` | `modules/acsd_p1_cosmetic.so` | unit `MOD-P1-COS`；`src/module_entry.cpp`；装配 selftest |
| p1 hips_writer | `lib/algorithms/drizzle/hips` | `modules/acsd_p1_hips_writer.so` | unit `MOD-P1-HIPSW`；`src/module_entry.cpp`；装配 selftest |
| cpu baseline provider | `lib/infrastructure/benchmark/backend_host` | `providers/acsd_cpu_baseline.so` | unit `PROV-CPU-BASELINE`；`baseline_backend.cpp` / `backend_loader.cpp` |
| CLI 平台单元 | `lib/infrastructure/cli/` | `acsd`（Windows `acsd.exe`） | unit `PLATFORM-CLI`；`parser.cpp` 的 `kRuleViews` 镜像命令树（`command_tree.h`）；判据 = 命令树与 `../contracts/CLI_PROTOCOL.md`「命令树」一节的一致性 |
| runtime / io 平台单元 | `lib/infrastructure/scheduler`、`lib/infrastructure/aio/io` | `libacsd_runtime.so` / `libacsd_io.so`（Windows `acsd_runtime.dll` / `acsd_io.dll`） | units `PLATFORM-RUNTIME` / `PLATFORM-IO`；安装树契约见 `eng/packaging/install-tree.contract.json` |

> 安装面唯一源：`eng/cmake/install_layout.cmake`（五科学模块 SHARED + `$ORIGIN` RPATH）
> + `eng/packaging/acsd.product.json`（units）+ `eng/packaging/install-tree.contract.json`。
> 双平台产物名以实际构建产物为准（Linux `.so` / Windows `.dll`）。

### C ABI 动态装载通道的可达性

本节区分动态装载接口与构建内调度入口。判断命令是否实际调用入口，需要从 CLI 沿生产路径阅读；安装声明、符号存在与装载探针分别提供不同范围的证据。

- **静态调用范围**：`acsd_registry_open_v1`（`lib/infrastructure/pipeline/module_loader/module_registry.c`）
 在 `lib/**` 中未见调用点；模块注册装载经 `acsd_secure_loader_load_v1`
 （`secure_loader.c`，内含 `dlsym("acsd_module_query_v1")` 与
 `dlsym("acsd_provider_query_v1")`）不在三个生产命令（`cmd_session{1,2,3}_run`）
 的调用图上。⇒ 各模块 `module.yaml#entrypoint` 与
 `integration.json#dll.unique_entry` / `operations[].entry` 声明的九操作入口，
 **不由三个命令在运行期 dlopen**。
- **生产运行面的模块注册表是构建内的**：`acsd::ModuleRegistry`
 （`lib/include/acsd/core/module.h`）+ 节点适配器表
 （`lib/infrastructure/scheduler/src/module_adapters.cpp`，见 「会话与节点执行面」一节），已接线；
 节点绑定需沿实际 operation 核对，不能由登记表推断全部节点的运行行为。
 交付与安装面应由安全装载器探针逐单元核对；具体执行需有命令和输出记录。
- **安装与加载检查的范围**：按安装清单逐 unit 检查科学模块和 noop；正路径核对
 sha256、module_id 和 allowed_root，负例覆盖 HASH_MISMATCH、MODULE_ID_MISMATCH、
 PATH_ESCAPE 和 FILE_MISSING；同时核对模块合同、ABI 和安装树产品清单。这些是检验意图，
 不是本页已运行的结果。
- 接口与可达性变化同步实际调用、模块说明和受影响消费者，不新增变更单审批。
- 本篇只承载模块到目录、产物与证据锚的映射。平台运行时与 I/O 平台单元的实现状态，
  声明面是 `eng/packaging/acsd.product.json` 与 `lib/infrastructure/pipeline/module_loader/README.md`，
  未确认事实和需裁决事项按其性质进入实现差异或未决登记。

## 会话与节点执行面（构建内，非独立 DLL）

| 模块 | 路径 | 职责 | 证据锚 |
| --- | --- | --- | --- |
| 运行时与唯一执行器 | `lib/infrastructure/scheduler` | 类型化有向无环图调度、线程预算租约、进程唯一 worker 池 | `lib/infrastructure/scheduler/src/executor_runtime.h`、`lib/infrastructure/scheduler/src/module_adapters.cpp` |
| 模块注册表（三阶段节点） | `lib/infrastructure/scheduler` | `normalize` 八节点（校准、修饰、天体定位、星点与 PSF、测光、噪声信噪比、球面重采样、产品写出）、`mosaic` 七节点（覆盖、采样、信号面拟合、信号面施加、排异、集成、写出）、`export` 五节点（properties、WCS、重采样、写出、验证），各绑真实 operation，具体依赖按生产 DAG 核对 | `lib/infrastructure/scheduler/src/module_adapters.cpp` |
| `normalize` 局部会话 | `lib/phase1_session` | `io_read → calibrate → cosmetic → io_write`，保留局部校准/兼容身份；完整命令走八节点适配器 | `lib/phase1_session/p1_session.cpp`（`manifest["stages"]`）；unit `entrypoint: p1_session_run`；生产主链见[6] |
| `normalize` 科学内核 | `lib/algorithms/photometry`、`lib/algorithms/star_detection`、`lib/algorithms/psf`、`lib/algorithms/platesolve`、`lib/algorithms/calibration`、`lib/algorithms/cosmetic`、`lib/algorithms/noise_snr` | 校准、检测 / PSF、天文定位、测光定标、噪声模型 | 各目录 `src/` 内真实源文件（逐内核 operation 见 `module_adapters.cpp`） |
| `normalize` 球面参考与序列化 | `lib/algorithms/noise_snr`、`lib/algorithms/integration/normalize_science` | noise 库完成球面参考 GLS；模型、sidecar 读取与发布由序列化库提供 | `sparse_reference.cpp::complete_spherical_reference`、`normalize_science.cpp`；由 `normalize_precision.cpp` 和 writer 接线；实现覆盖范围见[6] |
| `mosaic` 会话 | `lib/phase2_session` | 七节点链组装（coverage → sample → upm-fit → upm-apply → reject → integrate → write） | `lib/phase2_session/p2_session.cpp` |
| `mosaic` 内核 | `lib/algorithms/coverage` | `lib/algorithms/coverage/src` 下 coverage / sampler / upm / rejection / integrate / stage2_common 源文件 | 同上 + `eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json` |
| `export` 会话与内核 | `lib/phase3_session`、`lib/algorithms/projection`、`lib/algorithms/resample`、`lib/algorithms/fits_output` | 会话编排、目标 WCS、重采样与 FITS 写出；实际支持范围和事务按调用核对 | `lib/phase3_session/p3_session.cpp`、`lib/algorithms/projection/p3_wcs.cpp`、`lib/algorithms/resample/p3_resample.cpp`、`lib/algorithms/fits_output/p3_output.cpp`；构建内命令由 `P3NodeModule` 分派 |
| 三阶段产品交换 | `lib/infrastructure/aio/runtime/artifact_store` | 跨阶段仅磁盘产品交换（角色与类型强绑定） | `eng/contracts/data/phase_product_exchange.schema.json` + `lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py` |
| 结构化日志 | `lib/infrastructure/observability/logging` | JSONL 事件合同 | `../resources/observability/STRUCTURED_LOGGING.md` + 其 schema |
| 监控与资源读数 | `eng/tools/monitoring` | 资源测量与诊断，区分实测与配置阈值 | `eng/tools/monitoring/run_monitored.py`；不据工具名称增加研发门禁 |
| 统一图像 I/O | `lib/infrastructure/aio` | FITS / XISF / HiPS 读写、唯一 AIO C ABI v1 | `lib/infrastructure/aio/src/aio_abi.cpp`（编入生产 target `acsd_aio`） |
| HEALPix 与 drizzle 内核 | 生产实现 = `lib/algorithms/drizzle/healpix_drizzle`；归档面 = `lib/infrastructure/aio/healpix_db/archive`（`healpix_io` 与 `healpix_browser_qt` 不重建） | HEALPix 球面重采样、drizzle 累加与归并 | `lib/algorithms/drizzle/healpix_drizzle`；归档目录 `lib/infrastructure/aio/healpix_db/archive` |
| 公共工具 | `lib/algorithms/shared` | HEALPix 核心、SHA-256、计算 traits（仅头文件加静态） | `lib/algorithms/shared` |

## 合同 / 待接入目标目录（尚无独立 DLL 产物）

| 待接入目标 | 路径 | 现状与去向 |
| --- | --- | --- |
| p3 projection | `lib/algorithms/projection` | 投影数学与适用域由科学正本定义；`p3_projection_registry.h` 和 `p3_wcs.cpp` 是实现定位入口。实际支持范围需由生产调用核实，候选或注册名不能自动标为可用；DLL 与会话接入分别报告 |
| p3 resample | `lib/algorithms/resample` | 目标产物 `acsd_p3_resample`；生产实现在 `lib/algorithms/resample/p3_resample.cpp`；顶层占位 descriptor `acsd.phase3.resample` 接入归待接入目标 P3 重采样接入 |
| p3 fits | `lib/algorithms/fits_output` | 目标产物 `acsd_p3_fits`；生产实现在 `lib/algorithms/fits_output/p3_output.cpp`；适配器已使用流式写出与验证路径，独立 DLL 装载和全阶段内存上限分别核对 |
| phase2 upm / samp / rej / int | `lib/algorithms/upm`、`lib/algorithms/sampling`、`lib/algorithms/rejection`、`lib/algorithms/integration` | 生产实现在 `lib/algorithms/coverage`（节点化已在役）；独立 DLL 化为待接入目标 |
| hips_p2 | `lib/algorithms/coverage/hips_p2` | Phase2 HiPS 写出目标；生产路径在 `lib/infrastructure/aio` + `lib/algorithms/coverage` |
| plate_solve / photometric_calib / star_detector / dynamic_psf | `lib/algorithms/platesolve`、`lib/algorithms/photometry`、`lib/algorithms/star_detection`、`lib/algorithms/psf` | 已作为 Phase1 节点唯一真实 operation 接入（`module_adapters.cpp`）；独立 DLL 化未做 |

## 非交付面

| 目录 | 说明 |
| --- | --- |
| `lib/infrastructure/pipeline/orchestrator` | Phase1 编排已并入 CLI pipeline driver，无独立 exe；非生产入口 |
| `lib/infrastructure/hips_browser/healpix_browser_qt` | 球面浏览器（可选，不入产品 manifest）；工具分类，非发布 |
| `lib/algorithms/noise_snr` | 子目录未 `add_subdirectory`，其源集由根构建图直接编入 `acsd_phase1_noise` 目标（静态库，随链接闭包进产品图）；该子目录自带 target 不在根图 |
| `aio_pipeline_engine` 越权编排 | `lib/infrastructure/aio/src/aio_pipeline_engine.cpp` 在位但不由生产命令驱动 |
| `lib/algorithms/projection` 的 `p3_proj.h` / `p3_proj.cpp` | 不以文件存在认定其已进入生产闭包；实际编译源集由构建图核对，投影支持由生产调用及 `p3_projection_registry.h` 一并解释 |

## 每模块详细文档

[模块导航](../../detail/registry/README.md)指向三个阶段的 modules.md 与共用 HiPS writer；生产模块集合需与真实 descriptor 和调用图核对。

## 文档符号归属

路径指向真实文件或明确标识的目标位置，符号引用真实公开头或实现。反引号也可表达字段、命令、数学标识和状态，不要求每个词都被当作仓内路径或导出符号。存在性可自动辅助，职责、单位和调用关系仍需阅读正文及源码。

## 参考文献

[1] [ACSD 最高设计](../../ACSD_DESIGN.md)。

[2] [系统架构](ARCHITECTURE.md)。

[3] [生产构建图](../build/BUILD_GRAPH.md)。

[4] [命名块与端口合同](../contracts/PIPELINE_BLOCK.md)。

[5] [追溯规范](../governance/DOCUMENT_GOVERNANCE.md)。

[6] [Normalize 生产链与模块责任](NORMALIZE_PIPELINE.md)。
