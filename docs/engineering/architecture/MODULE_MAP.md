# ACSD 模块地图

本文是 `docs/engineering/` 的模块登记正本，逐行登记模块到目录、产物与证据锚的映射。
架构边界见同目录的 `ARCHITECTURE.md`，构建图见 `../build/BUILD_GRAPH.md`。
上游：`../../ACSD_DESIGN.md` 的软件架构与状态阶梯两章。

交付状态词表以 `../../ACSD_DESIGN.md` 的「状态阶梯」一节为唯一口径，本表不另立阶梯；
状态由检查与验收现场计算并记入产品 manifest，登记表不预写状态字段。本表只承载模块到目录、
产物与证据锚的映射，每行按 `lib/` 实际目录登记，对应模块页在 `docs/detail/registry/` 下。

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

本节登记动态装载通道在生产面的可达性事实。核对项 = 插件入口在生产面不可达，
以及已声明不可达的注册项（`manifest.entrypoint` 与 `integration.{op_entry,unique_entry}`）。

- **可达性事实**：`acsd_registry_open_v1`（`lib/infrastructure/pipeline/module_loader/module_registry.c`）
 在 `lib/**` 生产源零调用者；其唯一宿主解析点 `acsd_secure_loader_load_v1`
 （`secure_loader.c`，内含 `dlsym("acsd_module_query_v1")` 与
 `dlsym("acsd_provider_query_v1")`）不在三个生产命令（`cmd_session{1,2,3}_run`）
 的调用图上。⇒ 各模块 `module.yaml#entrypoint` 与
 `integration.json#dll.unique_entry` / `operations[].entry` 声明的九操作入口，
 **不由三个命令在运行期 dlopen**。
- **生产运行面的模块注册表是构建内的**：`acsd::ModuleRegistry`
 （`lib/include/acsd/core/module.h`）+ 节点适配器表
 （`lib/infrastructure/scheduler/src/module_adapters.cpp`，见 「会话与节点执行面」一节），已接线；
 节点绑定的核对（每节点唯一真实 operation）由人读对抗性审核逐条执行。
 「交付面」一节 的五个科学模块动态库属交付与安装面，由安全装载器探针逐单元装配验证。
- **消费者（安装与加载面）**：
 - 安全 loader 逐 unit 加载 5 个科学模块
 DLL + noop，正路径（sha256 / module_id / allowed_root 三校验）与 4 类负路径
 （HASH_MISMATCH / MODULE_ID_MISMATCH / PATH_ESCAPE / FILE_MISSING）必败；
 - module 合同与 ABI 三方一致正测；
 - 安装树产品清单核对。
- 可达面收缩须经 `docs/detail/registry/` 的变更单，按最高设计的顶层结构章走变更流程。
- 本篇只承载模块到目录、产物与证据锚的映射。平台运行时与 I/O 平台单元的实现状态，
  声明面是 `eng/packaging/acsd.product.json` 与 `lib/infrastructure/pipeline/module_loader/README.md`，
  状态类结论由 `../governance/UNRESOLVED.md` 承载。

## 会话与节点执行面（构建内，非独立 DLL）

| 模块 | 路径 | 职责 | 证据锚 |
| --- | --- | --- | --- |
| 运行时与唯一执行器 | `lib/infrastructure/scheduler` | 类型化有向无环图调度、线程预算租约、进程唯一 worker 池 | `lib/infrastructure/scheduler/src/executor_runtime.h`、`lib/infrastructure/scheduler/src/module_adapters.cpp` |
| 模块注册表（三阶段节点） | `lib/infrastructure/scheduler` | `normalize` 八节点（校准、修饰、天体定位、星点与 PSF、测光、噪声信噪比、球面重采样、产品写出）、`mosaic` 七节点（覆盖、采样、天光面拟合、天光面施加、排异、集成、写出）、`export` 五节点（properties、WCS、重采样、验证、写出），各绑唯一真实 operation（节点序以最高设计各阶段流程章为准） | `lib/infrastructure/scheduler/src/module_adapters.cpp` |
| `normalize` 会话 | `lib/phase1_session` | `io_read → calibrate → cosmetic → io_write` | `lib/phase1_session/p1_session.cpp`（`manifest["stages"]`）；unit `entrypoint: p1_session_run` |
| `normalize` 科学内核 | `lib/algorithms/photometry`、`lib/algorithms/star_detection`、`lib/algorithms/psf`、`lib/algorithms/platesolve`、`lib/algorithms/calibration`、`lib/algorithms/cosmetic`、`lib/algorithms/noise_snr` | 校准、检测 / PSF、天文定位、测光定标、噪声模型 | 各目录 `src/` 内真实源文件（逐内核 operation 见 `module_adapters.cpp`） |
| `mosaic` 会话 | `lib/phase2_session` | 七节点链组装（coverage → sample → upm-fit → upm-apply → reject → integrate → write） | `lib/phase2_session/p2_session.cpp` |
| `mosaic` 内核 | `lib/algorithms/coverage` | `lib/algorithms/coverage/src` 下 coverage / sampler / upm / rejection / integrate / stage2_common 源文件 | 同上 + `eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json` |
| `export` 会话 | `lib/phase3_session` | properties / WCS（TAN）/ nearest+bilinear 重采样 / CFITSIO 原子写 / verify | `lib/phase3_session` 的 p3_session / p3_wcs / p3_resample / p3_output 四源文件 |
| 三阶段产品交换 | `lib/infrastructure/aio/runtime/artifact_store` | 跨阶段仅磁盘产品交换（角色与类型强绑定） | `eng/contracts/data/phase_product_exchange.schema.json` + `lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py` |
| 结构化日志 | `lib/infrastructure/observability/logging` | JSONL 事件合同 | `../resources/observability/STRUCTURED_LOGGING.md` + 其 schema |
| 监控与资源判据 | `eng/tools/monitoring` | 冻结阈值判定（阈值唯一数值源 `eng/contracts/resource_gate_v1.json`） | `eng/tools/monitoring/run_monitored.py` 的冻结阈值判定函数 |
| 统一图像 I/O | `lib/infrastructure/aio` | FITS / XISF / HiPS 读写、唯一 AIO C ABI v1 | `lib/infrastructure/aio/src/aio_abi.cpp`（编入生产 target `acsd_aio`） |
| HEALPix 与 drizzle 内核 | 生产实现 = `lib/algorithms/drizzle/healpix_drizzle`；归档面 = `lib/infrastructure/aio/healpix_db/archive`（`healpix_io` 与 `healpix_browser_qt` 不重建） | HEALPix 球面重采样、drizzle 累加与归并 | `lib/algorithms/drizzle/healpix_drizzle`；归档目录 `lib/infrastructure/aio/healpix_db/archive` |
| 公共工具 | `lib/algorithms/shared` | HEALPix 核心、SHA-256、计算 traits（仅头文件加静态） | `lib/algorithms/shared` |

## 合同 / 待接入目标目录（尚无独立 DLL 产物）

| 待接入目标 | 路径 | 现状与去向 |
| --- | --- | --- |
| p3 projection | `lib/algorithms/projection` | 设计冻结 8 种投影（`TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA`，最高设计的投影算法一节）；**在役 registry = `p3_projection_registry.h`**（header-only inline，由同目录 `p3_wcs.cpp` 直接包含），可作产品声明的投影以该注册表的当前声明为权威，未实现项报「不支持」；DLL 挂载与生产会话切换归待接入目标 P3 投影接入 |
| p3 resample | `lib/algorithms/resample` | 目标产物 `acsd_p3_resample`；生产实现在 `lib/algorithms/resample/p3_resample.cpp`；顶层占位 descriptor `acsd.phase3.resample` 接入归待接入目标 P3 重采样接入 |
| p3 fits | `lib/algorithms/fits_output` | 目标产物 `acsd_p3_fits`；生产实现在 `lib/algorithms/fits_output/p3_output.cpp`；流式 FITS 接入未实现 |
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
| `lib/algorithms/projection` 的 `p3_proj.h` / `p3_proj.cpp` | v6 内核行：无构建目标编译其源文件，不进产品图；在役产品声明 registry 是同目录 header-only 的 `p3_projection_registry.h` |

## 每模块详细文档

`docs/detail/registry/<module>.md`（L5 模板）；模块清单以本表 「交付面」一节–「非交付面」一节 为准。
## 文档符号归属

文档正文反引号里的 token 只能指代两类对象之一：仓库内可解析的文档词干与路径，或
`lib/**` 与 `lib/include/**` 公开头中真实定义的符号。凡两类都不解析的 token 一律改写为
本表可解析的写法（完整路径、公开头符号名，或具名的判据词）。该口径的核对由人读对抗性审核
逐条执行，见 `../governance/DOCUMENT_GOVERNANCE.md` 的引用与登记纪律。

## 参考文献

[1] 内部文档 `../../ACSD_DESIGN.md`，最高设计的软件架构与状态阶梯两章。

[2] 内部文档 `ARCHITECTURE.md`，系统架构。

[3] 内部文档 `../build/BUILD_GRAPH.md`，生产构建图。

[4] 内部文档 `../contracts/PIPELINE_BLOCK.md`，命名块生命周期与端口双向一致判据。
