# Module Map

> 上游：`docs/ACSD_DESIGN.md` §8（软件架构）、§12.5（状态阶梯）

状态词口径 = `docs/ACSD_DESIGN.md` §12.5（不另立阶梯）；本文件的表格**不写状态字段**——
各面状态由交付状态**现场计算**（载体见 门禁注册面（G08-10 重建））并落在
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
（注册面见 门禁注册面（G08-10 重建））的 `W6 plugin_entry_unreachable` 与
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
  - 安装树产品清单核对（载体见 门禁注册面（G08-10 重建））。
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
