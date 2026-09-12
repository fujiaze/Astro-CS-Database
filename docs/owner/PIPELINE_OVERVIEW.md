# 管线总览（Pipeline Overview）

> 文档 ID：DOC-GOV-OWNER-PIPELINE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003）
> 建立基线：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`（GOV-004，历史值）
> 收敛基线：DOC-CONV-001，BASE_SHA = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`
> 权威：`AstroCS_ENGINEERING_CONSTRAINTS.md` §A（产品与阶段）、
> `docs/contracts/{ARCH-001,RT-001,DATA_ARTIFACTS,DATA_SEMANTICS}.md`、
> `docs/api/{PHASE1_API_V1,PHASE2_API_V1,PHASE3_API_V1,CLI_PROTOCOL_V1}.md`、
> `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md`。
> 状态词约定同 RELEASE_STATUS §0（DOC-CONV-001 唯一口径）：
> `CONTRACT_READY` / `IMPLEMENTED` / `INSTALLED` / `VERIFIED` /
> `NOT_IMPLEMENTED` / `NOT_VERIFIED` / `DEFERRED` / `DORMANT` / `FAIL`；
> `IMPLEMENTED` 要求给出当前提交内的实测命令与 rc。

## 1. 三 Phase 隔离模型（验收项：三 Phase 隔离）

约束 §A.2-A.6 冻结的产品模型：**Phase1 / Phase2 / Phase3 是三个隔离的产品命令，
不是固定顺序流水线**：

```text
Phase1: 单帧 light + masters/catalog/config
        → 单帧标准化 HiPS + manifest
Phase2: 任意一组合同兼容 HiPS
        → 马赛克 HiPS + UPM/rejection/integration provenance
Phase3: 任一合同兼容 HiPS（不要求来自 Phase2）
        → 平面 FITS + WCS/coverage/validity/provenance
```

- 阶段间只通过原子发布、哈希与 provenance 完整的磁盘产品/manifest 交换（§A.6；DATA-002）。
- 禁止同进程 `--phases 1,2,3`；外部脚本可显式依次启动三个独立进程（§A.4）。
- Phase3 不得假设输入来自 Phase2；Phase2 不得假设输入由同一进程 Phase1 产生（§A.5）。

### 现状核实（源码在位）

| 隔离要求 | 状态 | 依据（当前提交内静态可核） |
|---|---|---|
| 独立命令 `phase1/2/3 run` + `validate/plan/inspect` 存在 | `INSTALLED` | `cli/parser.cpp` kRules 登记 phase1/2/3 × {validate,plan,inspect,run} 共 12 条；`build/cli/astrocs --help` 本提交实测列出；`tests/cli/test_cli001_vpi.py` 15/15 PASS |
| **遗留 `astrocs run --phases 1,2,3` 进程内连跑** | **已删除（`IMPLEMENTED`）** | CLI-002 移除 `run`/`graph` 入口（`a6c39cc1` 收口未知命令判定）；本提交实测 `astrocs run --phases 1,2,3` → rc=2 `unknown command 'run'`，与约束 §A.4 一致 |
| 单 Phase 命令走独立进程/独立 Runtime 实例 | `IMPLEMENTED` | `cmd_phase1_run/cmd_phase2_run/cmd_phase3_run`（`cli/commands.cpp`）各启动单 Phase 运行 |
| RT-001 类型化 DAG 拒绝跨 Phase edge | PASS | `runtime/pipeline/typed_dag.py` + `module_ports.registry.json`（module 带 phase 字段，跨 Phase edge 拒绝，见 RT-001 集成 commit requirements） |
| DATA-002 产品交换合同（磁盘交换、role↔type 绑定、Phase2 不依赖 Phase1 run ID） | PASS | `contracts/data/phase_product_exchange*` + `runtime/artifact_store/phase_product_exchange_validator.py`（合同冻结） |
| 三 Phase 隔离的执行验收（独立进程冒烟，当前提交复跑） | `NOT_VERIFIED` | 三 Phase 端到端独立进程冒烟未在本提交复跑；节点化/消费者用例（`p1001/p2001/p3002`）已实测绿，但不冒认端到端验收 |

## 2. Phase1 内部链（单 Phase IR 子图）

Phase1 目标链（03_TARGET_PRODUCT_AND_ARCHITECTURE.md §5）：
`read → calibration → cosmetic → star_detection → psf → wcs → photometry → noise
→ drizzle → hips_writer`。

当前基线的实际装配（BASE=`da3c4b4a`）：
- CLI 层 Phase1 IR：`cli/runtime_client.cpp`:92-114 两节点链
  `cal(astrocs.phase1.calibration) → cos(astrocs.phase1.cosmetic)`。
- 会话层：`lib/phase1_session/p1_session.cpp` 实现
  `io_read → calibrate → cosmetic → io_write`（`s->manifest["stages"]` 链）。
- 模块注册表：`lib/core/src/module_adapters.cpp`:4257 注册 Phase1 八节点
  `astrocs.phase1.{calibration,cosmetic,star-psf,wcs-platesolve,photometry,
  noise-snr,drizzle,writer}`，逐个经 `make_p1_node_module` 绑定**唯一真实
  operation**（`calibrate/cosmetic_correct/detect_sources/plate_solve/measure_flux/
  estimate_snr/drizzle_stack/write_hips`）。
- 模块端口注册表：`runtime/pipeline/module_ports.registry.json`（RT-001）登记
  `astrocs.phase1.*` 与 entry（`astrocs_phase1_*_v1`）。

状态：装配与节点绑定在位且**本提交实测通过**（`IMPLEMENTED`）——
ctest `p1001_real_nodes`（7 节点主链 cal→cos→psf→phot→snr→drz→wr，
每节点 call_count=1、fail-fast 下游零调用、确定性 bitwise）本提交 rc=0（P1-001 `9e09941a`）。

> **约束 §F.1（每 DAG 节点唯一真实模块 operation）已在三 Phase 达成**：
> Phase1 八节点（本文件 §2）、Phase2 七节点（§3）、Phase3 五节点（§4）全部由
> `make_p1/p2/p3_node_module` 绑定唯一真实 operation，不再出现"多节点重复调用
> 同一个完整 phaseN session"的中间态。绑定表唯一源 =
> `runtime/pipeline/module_ports.registry.json`（operation/entry 与
> `module_adapters.cpp` 逐一一致），由节点化 ctest 用例（`p1001/p2001/p3002`）
> 在本提交实测复核。

## 3. Phase2 内部链

目标链：`input_manifest → coverage → sampling → upm_fit → upm_apply → rejection
→ integration → hips_writer`（03 §5）。

当前基线装配：
- `cli/runtime_client.cpp` `phase2_nodes()`：7 节点链
  `coverage → sample → upm_fit → upm_apply → reject → integrate → write`。
- `lib/phase2_session/p2_session.cpp`：coverage→sample→upm→reject→integrate→write
  的进程内链（stages 记录）。
- `lib/phase2/src/{coverage,sampler,upm,rejection,integrate,stage2_common}.cpp` 实现在位。
- `module_ports.registry.json`：`astrocs.phase2.{coverage,sample,upm-fit,upm-apply,
  reject,integrate,write}`（phase=phase2）。

状态：`IMPLEMENTED` —— 七节点各绑唯一真实 operation（`lib/core/src/module_adapters.cpp`:4282），
ctest `p2001_real_nodes`、`p2002_unc_rej_prov` 本提交实测 rc=0
（P2-001 `439f9f20`、P2-002 `9e0fa3a8`）；真实数据（合成/接缝/Windows）验收
未在本提交复跑（`NOT_VERIFIED`）。

## 4. Phase3 内部链

目标链（03 §5）：`input_hips → projection_plan → resample_blocks → fits_stream_writer`
（投影/重采样/写出是真实独立节点，禁止三个节点重复调用完整 p3_session_run —— 属 W4 删除范围）。

当前基线装配：
- `cli/runtime_client.cpp` `phase3_nodes()`：5 节点链
  `properties → wcs → resample2 → writer → verify`。
- `lib/phase3_session/`：p3_session 组装 properties/WCS/采样/原子写（p3_session.cpp 注释
  明示 P3-001..P3-004 组装）；会话路径 WCS 仍 TAN-only（`p3_wcs.cpp`:36）。
- `lib/phase3_session/p3_resample.{h,cpp}`：nearest / bilinear（G4 冻结权重，
  `p3_uncertainty_propagate`）——**无 healpix_interp4**（`NOT_IMPLEMENTED`，
  见 SCIENCE_OVERVIEW §4）。
- `lib/phase3_proj/p3_projection.{h,cpp}`：**冻结四投影 TAN/SIN/CAR/AIT registry v1**
  （`:267-273`，宪章 §7.3/§18.1），统一操作面 make/pix2world/world2pix/fits_keywords；
  现状为注册测试面（`ctest p3_projection_units/p3_projection_fault` 本提交 2/2 PASS），
  会话/DLL 挂载未切换（`lib/phase3_proj/module.yaml`:79-80 `entrypoint: MISSING`）。
- Phase3 writer 现走 CFITSIO 原子写（p3_output.cpp），**未接入 IO-001 流式 FITS**
  （`NOT_IMPLEMENTED`）。
- `module_ports.registry.json`：`astrocs.phase3.{properties,wcs,resample2,writer,verify}`。

状态：`IMPLEMENTED` —— 五节点各绑唯一真实 operation（`lib/core/src/module_adapters.cpp`:4309，
typed artifact 链经 output_dir 文件约定传递，上游缺失 fail-closed），
ctest `p3002_real_nodes`/`p3002_uncertainty` 本提交实测 rc=0（P3-002 `1a56ffb7`）；
`healpix_interp4` 与流式 FITS 接入 `NOT_IMPLEMENTED`；
「投影/重采样/写出为独立节点、非重复调用完整 p3_session_run」**已达成**（§F.1，同 §2 注）。

## 5. 执行与运行图

- `astrocs phaseN run` 经 `cli/runtime_client.cpp run_pipeline()` →
  `astrocs_core` Runtime（`include/astrocs/core/runtime.h`、`lib/core/src/runtime.cpp`）
  单共享 executor + ThreadBudget（约束 §D.3；RT-001 合同）。
- IR 形态：`astrocs.pipeline/v1` JSON；每次 run 应产出 run-plan/graph/trace 等。
  运行图静态/观测产物生成是否完整属 W5/LNX 域，不在本任务复跑。
- 唯一 executor 与实测资源门（RT-001 `91440c16`）：`lib/core/src/executor_runtime.h`
  + `lib/core/src/module_adapters.cpp`:3777-3793（Phase3 resample 行带提交唯一池，
  每任务经 `ThreadBudget acquire(1,1)` 恰租 1 槽）；`tools/monitoring/run_monitored.py`
  `evaluate_frozen_gate()` 按宪章 §10.5/§18.2 冻结阈值（均值≥85%、任何连续 10s<60%、
  单活跃线程即 fail）判定，未请求 `--gate-required` 时零行为变化，
  缺失/非法监控输入 fail-closed。ctest `rt001_unique_executor` 本提交实测 rc=0。

## 6. 与发布形态的关系

- 用户命令面以 `docs/api/CLI_PROTOCOL_V1.md`（API-CLI-001 冻结）为准：
  `phase1/2/3 run`、`verify`、`benchmark cpu`、`doctor` 等。
- 目标发布安装树（03 §4）：`astrocs.exe` + runtime/io/科学模块/provider DLL；
  HiPS Browser、ACR/CUDA 不入 product manifest。当前根 CMake（BLD-002）唯一
  `add_executable(astrocs)` 显式链接各静态库；**Linux 技术预览安装面已 `INSTALLED`**
  （`cmake/install_layout.cmake`:104-105 五科学模块入 `modules/`，
  `packaging/astrocs.product.json` units=10，安全 loader 实测 64/64 PASS），
  **Windows 侧安装树复验 `NOT_VERIFIED`**（`docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv`
  等为 GENERATED 清单，见 ARCHITECTURE_OVERVIEW）。

## 7. 管线状态汇总

```text
三 Phase 独立命令:            INSTALLED（phase1/2/3 × validate|plan|inspect|run）
遗留 run --phases 连跑:        已删除 IMPLEMENTED（实测 rc=2 unknown command）
跨 Phase 仅磁盘交换合同:        CONTRACT_READY（DATA-002 冻结）
RT-001 类型化 DAG 跨 Phase 拒绝: IMPLEMENTED
单 Phase 内部链装配与源码:      IMPLEMENTED（P1/P2/P3 各自链 + 节点绑定在位）
每节点唯一 operation 绑定:      IMPLEMENTED（P1 8 / P2 7 / P3 5 节点，ctest 实测）
RT 唯一 executor + 实测资源门:  IMPLEMENTED（rt001_unique_executor 实测 + §10.5 阈值）
Phase3 冻结四投影 registry:     IMPLEMENTED（TAN/SIN/CAR/AIT；生产挂载 entrypoint=MISSING）
单 Phase 执行验收（当前提交复跑）: NOT_VERIFIED（端到端独立进程冒烟未复跑）
Phase3 扩展（healpix_interp4、流式 FITS 接入）: NOT_IMPLEMENTED
```

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
convergence_task: DOC-CONV-001
convergence_base_sha: da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540
