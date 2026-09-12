# Module Map

> 文档 ID：DOC-ARCH-MODULE-MAP
> 状态：ACTIVE_INFORMATIVE（DOC-CONV-001 按 BASE=`da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540` 实际收敛）
> 状态词：CONTRACT_READY / IMPLEMENTED / INSTALLED / VERIFIED /
> NOT_IMPLEMENTED / NOT_VERIFIED（唯一口径见 `docs/owner/RELEASE_STATUS.md` §0）。
> 本表按 **`lib/` 实际目录** 登记（不再描述已不存在或尚未建立的产物）；
> 每行给出可核证据锚。详细 L5 文档见 `docs/modules/`。

## 1. 交付面（进构建/安装树）

| 模块 | 路径 | 交付状态 | 产物 | 证据锚 |
| --- | --- | --- | --- | --- |
| conformance (noop) | `lib/core`（target 定义在根构建） | INSTALLED | `modules/astrocs_noop.so` | `packaging/astrocs.product.json` unit `MOD-NOOP` = `astrocs.conformance.noop`；安全 loader 实测加载（`tests/abi/mod001_install_load_check.py` 64/64） |
| catalog gaia | `lib/gaia_xpsd_client` | INSTALLED | `modules/astrocs_catalog_gaia.so` | unit `MOD-CAT-GAIA` = `astrocs.catalog.gaia`；`src/module_entry.c` 九操作 + `astrocs_module_query_v1`；实测 selftest 装配 PASS |
| p1 drizzle | `lib/drizzle` | INSTALLED | `modules/astrocs_p1_drizzle.so` | unit `MOD-P1-DRIZZLE`；`src/module_entry.cpp`（C ABI v1 九操作）；实测装配 PASS |
| p1 calibration | `lib/calibration` | INSTALLED | `modules/astrocs_p1_calibration.so` | unit `MOD-P1-CAL`；`src/module_entry.cpp`；实测装配 PASS |
| p1 cosmetic | `lib/cosmetic` | INSTALLED | `modules/astrocs_p1_cosmetic.so` | unit `MOD-P1-COS`；`src/module_entry.cpp`；实测装配 PASS |
| p1 hips_writer | `lib/hips` | INSTALLED | `modules/astrocs_p1_hips_writer.so` | unit `MOD-P1-HIPSW`；`src/module_entry.cpp`；实测装配 PASS |
| cpu baseline provider | `lib/backend_host` | INSTALLED | `providers/astrocs_cpu_baseline.so` | unit `PROV-CPU-BASELINE`；`baseline_backend.cpp`/`backend_loader.cpp` |
| CLI 平台单元 | `cli/` | INSTALLED | `astrocs` | unit `PLATFORM-CLI`；`cli/parser.cpp` kRules 12 条 phase 命令；`tests/cli/test_cli001_vpi.py` 15/15 实测 |
| runtime / io 平台单元 | `lib/core`、`lib/io` | INSTALLED（骨架） | `libastrocs_runtime.so`、`libastrocs_io.so` | units `PLATFORM-RUNTIME`/`PLATFORM-IO` 状态 = SKELETON（不冒认实现完成度） |

> 安装面唯一源：`cmake/install_layout.cmake`（:104-105 五科学模块 SHARED + `$ORIGIN`
> RPATH）+ `packaging/astrocs.product.json`（units=10）+ `packaging/install-tree.contract.json`。
> 当前为 **Linux 技术预览** 安装面；Windows 侧复验 NOT_VERIFIED。

## 2. 会话与节点执行面（构建内，非独立 DLL）

| 模块 | 路径 | 交付状态 | 职责 | 证据锚 |
| --- | --- | --- | --- | --- |
| Runtime / 唯一 executor | `lib/core` | IMPLEMENTED | typed DAG 调度、ThreadBudget 租约、进程唯一 worker 池 | `lib/core/src/executor_runtime.h`、`lib/core/src/module_adapters.cpp`:3777-3793；ctest `rt001_unique_executor` 实测 PASS |
| 模块注册表（三 Phase 节点） | `lib/core` | IMPLEMENTED | P1 八节点 / P2 七节点 / P3 五节点，各绑唯一真实 operation（宪章 §F.1） | `lib/core/src/module_adapters.cpp`:4257/:4282/:4309；ctest `p1001_real_nodes`/`p2001_real_nodes`/`p3002_real_nodes`/`p3002_uncertainty` 4/4 实测 |
| Phase1 会话 | `lib/phase1_session` | IMPLEMENTED | `io_read → calibrate → cosmetic → io_write` | `lib/phase1_session/p1_session.cpp`（`manifest["stages"]`）；unit `entrypoint: p1_session_run` |
| Phase1 科学内核 | `lib/phase1` | IMPLEMENTED | noise / photometry / stars / wcs 子目录内核 | `lib/phase1` 下 noise/photometry/stars/wcs 子目录 |
| Phase2 会话 | `lib/phase2_session` | IMPLEMENTED | 七节点链组装（coverage→sample→upm→reject→integrate→write） | `lib/phase2_session/p2_session.cpp`；ctest `p2001_real_nodes` 实测 |
| Phase2 内核 | `lib/phase2` | IMPLEMENTED | `lib/phase2/src` 下 coverage/sampler/upm/rejection/integrate/stage2_common 源文件 | 同上 + `contracts/data/phase2_uncertainty_rejection_provenance_v1.json`（P2-002） |
| Phase3 会话 | `lib/phase3_session` | IMPLEMENTED | properties / WCS（TAN）/ nearest+bilinear 重采样 / CFITSIO 原子写 / verify | `lib/phase3_session` 的 p3_session/p3_wcs/p3_resample/p3_output 四源文件；ctest `p3002_real_nodes`/`p3002_uncertainty` 实测 |
| 三阶段产品交换 | `runtime/artifact_store` | CONTRACT_READY | 跨 Phase 仅磁盘产品交换（role↔type 强绑定） | `contracts/data/phase_product_exchange.schema.json` + `runtime/artifact_store/phase_product_exchange_validator.py` |
| 结构化日志 | `runtime/logging` | CONTRACT_READY | JSONL 事件合同 | LOG-001 schema/契约 |
| 监控与资源门 | `tools/monitoring` | IMPLEMENTED | 冻结阈值判定（§10.5/§18.2） | `tools/monitoring/run_monitored.py` `evaluate_frozen_gate()`；pytest `tests/monitoring` |
| AIO 图像 I/O | `lib/astro_image_io` | IMPLEMENTED | FITS/XISF/HiPS 读写、唯一 AIO C ABI v1 | `lib/astro_image_io/src/aio_abi.cpp`（编入生产 target `astrocs_aio`，MOD-001 实测握手 abi=1/status_count=71） |
| HEALPix / Drizzle 内核 | `lib/healpix_db` | IMPLEMENTED | `healpix_drizzle`（生产）+ `healpix_io`；`archive/legacy` 与 `healpix_browser_qt` 不重建 | `lib/healpix_db/healpix_drizzle`、`lib/healpix_db/archive` |
| 公共工具 | `lib/common` | IMPLEMENTED | HEALPix core / SHA-256 / compute traits（header-only + 静态） | `lib/common` |

## 3. 合同/迁移目标目录（尚无独立 DLL 产物）

| 迁移目标 | 路径 | 交付状态 | 现状与去向 |
| --- | --- | --- | --- |
| p3 projection | `lib/phase3_proj` | IMPLEMENTED（registry）/ `entrypoint: MISSING` | 冻结四投影 `TAN/SIN/CAR/AIT` registry v1（`lib/phase3_proj/p3_projection.cpp`:267-273）+ ctest `p3_projection_units`/`p3_projection_fault` 实测；DLL 挂载与生产会话切换归 P3-PROJ-INT；**未 INSTALLED** |
| p3 resample | `lib/phase3_rsmp` | CONTRACT_READY（合同目录） | 目标 `astrocs_p3_resample.dll`；生产实现在 `lib/phase3_session/p3_resample.cpp`；顶层占位 descriptor `astrocs.phase3.resample` 归 P3-RSMP-INT（DEFERRED） |
| p3 fits | `lib/phase3_fits` | CONTRACT_READY（合同目录） | 目标 `astrocs_p3_fits.dll`；生产实现在 `lib/phase3_session/p3_output.cpp`；流式 FITS 接入 NOT_IMPLEMENTED |
| phase2 upm / samp / rej / int | `lib/phase2_upm`、`lib/phase2_samp`、`lib/phase2_rej`、`lib/phase2_int` | CONTRACT_READY（合同目录） | 生产实现在 `lib/phase2`（节点化已 IMPLEMENTED）；独立 DLL 化为迁移目标 |
| hips_p2 | `lib/hips_p2` | CONTRACT_READY（合同目录） | Phase2 HiPS 写出目标；生产路径在 `lib/astro_image_io` + `lib/phase2` |
| plate_solve / photometric_calib / star_detector / dynamic_psf | `lib/plate_solve`、`lib/photometric_calib`、`lib/star_detector`、`lib/dynamic_psf` | IMPLEMENTED（节点内核） | 已作为 Phase1 节点唯一真实 operation 接入（`module_adapters.cpp`:4257）；独立 DLL 化未做（`entrypoint: MISSING` 属实） |

## 4. 非交付面

| 目录 | 状态 | 说明 |
| --- | --- | --- |
| `lib/acr` | DORMANT | 异构计算抽象；根 `CMakeLists.txt`:17 ACR 默认 OFF，生产 target 不链；保留源码与隔离测试 |
| `lib/orchestrator` | 历史保留 | Phase1 编排已并入 CLI pipeline driver，无独立 exe |
| `lib/healpix_db/healpix_browser_qt` | 工具分类（非发布） | HiPS 浏览器（optional，不入 product manifest） |
| `lib/snr_estimator` | 不在根构建图（如实） | F-CI-002-01（owner 裁决 2026-09-11）摘出 `add_subdirectory`；`tests/unit` 门卫为 `if(EXISTS)`/`if(TARGET)`，收编后自动恢复 |
| 旧 `aio_pipeline_engine` 越权编排 | 保留中（DEFERRED） | `lib/astro_image_io/src/aio_pipeline_engine.cpp` 仍在位，ARCH-001 §7 登记，LEG-003 迁移；不宣称已删除 |

## 5. 每模块详细文档

`docs/modules/<module>.md`（L5 模板）；模块清单以本表 §1–§4 为准。
