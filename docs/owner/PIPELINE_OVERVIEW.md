# 管线总览（Pipeline Overview）

> 文档 ID：DOC-GOV-OWNER-PIPELINE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003）
> 基线提交：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`
> 权威：`AstroCS_ENGINEERING_CONSTRAINTS.md` §A（产品与阶段）、
> `docs/contracts/{ARCH-001,RT-001,DATA_ARTIFACTS,DATA_SEMANTICS}.md`、
> `docs/api/{PHASE1_API_V1,PHASE2_API_V1,PHASE3_API_V1,CLI_PROTOCOL_V1}.md`、
> `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md`。
> 状态词约定同 SCIENCE_OVERVIEW：合同冻结 / 源码在位 / 执行验收 三级，未核实标 NOT_VERIFIED。

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
| 独立命令 `phase1 run / phase2 run / phase3 run` 存在 | PASS | `cli/parser.cpp` 命令白名单（kRules）含 `phase1 run/phase2 run/phase3 run/run` |
| **遗留 `astrocs run --phases 1,2,3` 进程内连跑**（多 Phase 单进程） | **FAIL（与约束 §A.4 冲突，未删除）** | `cli/parser.cpp`/`cli/commands.cpp` 仍实现 `run --phases`（parse 允许 1,2,3；`cmd_run_pipeline` 一次调度 P1/P2/P3 节点）→ 属于任务图 W4「删除伪/旧路由」遗留（02_CURRENT_BASELINE_AUDIT 亦登记）；本文如实记录，不宣称已删除 |
| 单 Phase 命令走独立进程/独立 Runtime 实例 | PASS | `cmd_phase1_run/cmd_phase2_run/cmd_phase3_run`（commands.cpp）各启动单 Phase 运行 |
| RT-001 类型化 DAG 拒绝跨 Phase edge | PASS | `runtime/pipeline/typed_dag.py` + `module_ports.registry.json`（module 带 phase 字段，跨 Phase edge 拒绝，见 RT-001 集成 commit requirements） |
| DATA-002 产品交换合同（磁盘交换、role↔type 绑定、Phase2 不依赖 Phase1 run ID） | PASS | `contracts/data/phase_product_exchange*` + `runtime/artifact_store/phase_product_exchange_validator.py`（合同冻结） |
| 三 Phase 隔离的执行验收（独立进程冒烟，当前提交复跑） | NOT_VERIFIED | 未在当前提交复跑；不冒充 |

## 2. Phase1 内部链（单 Phase IR 子图）

Phase1 目标链（03_TARGET_PRODUCT_AND_ARCHITECTURE.md §5）：
`read → calibration → cosmetic → star_detection → psf → wcs → photometry → noise
→ drizzle → hips_writer`。

当前基线的实际装配：
- CLI 层 Phase1 IR：`cli/runtime_client.cpp` `phase1_node()` 单节点
  `node_id=cal, module_id=astrocs.phase1.calibration`（预设单节点表示）。
- 会话层：`lib/phase1_session/p1_session.cpp` 实现
  `io_read → calibrate → cosmetic → io_write`（`s->manifest["stages"]` 链）。
- 模块注册表：`lib/core/src/module_adapters.cpp` `register_phase_modules()` 注册
  Phase1 模块族 `astrocs.phase1.{calibration,cosmetic,star-psf,wcs-platesolve,
  photometry,noise-snr,drizzle,writer}`（descriptor 端口/SCI/ALG/DATA/API/TEST 引用）。
- 模块端口注册表：`runtime/pipeline/module_ports.registry.json`（RT-001）登记
  `astrocs.phase1.*` 与 entry（`astrocs_phase1_*_v1` 声明名）。

状态：链上模块名与端口声明在位（PASS，静态可核）；链上真实执行验收未在当前提交复跑
（NOT_VERIFIED）。

> **约束 §F.1 缺口（如实记录，验收项相关）**：`lib/core/src/module_adapters.cpp`
> 中 Phase1/2/3 各子模块 descriptor 虽已注册，但其工厂全部委托
> `P1Api/P2Api/P3Api` 的 `*_session_run` —— 即 IR 上多个节点（如 Phase2 的
> coverage→…→write 七节点、Phase3 的 properties→…→verify 五节点）执行时
> **都调用同一个完整 phaseN session**（重复包装同一 Session），并非每节点绑定
> 唯一真实模块 operation。这正是 03_TARGET §5 禁止的"多个节点重复调用一个完整
> p3_session_run"中间态；RT-001 集成时 `module_ports.registry.json` note 亦声明
> entry 为声明名、真实 DLL 绑定属 ABI-00x/RT-002+。因此约束 §F.1 的"每节点唯一
> operation 绑定"**当前未达成（进行中，W3 模块化 + W4 删除伪路由范围）**，
> 不宣称已实现。

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

状态：声明与实现在位（PASS，静态可核）；执行验收（合成/接缝/Windows）未在当前
提交复跑（NOT_VERIFIED）。

## 4. Phase3 内部链

目标链（03 §5）：`input_hips → projection_plan → resample_blocks → fits_stream_writer`
（投影/重采样/写出是真实独立节点，禁止三个节点重复调用完整 p3_session_run —— 属 W4 删除范围）。

当前基线装配：
- `cli/runtime_client.cpp` `phase3_nodes()`：5 节点链
  `properties → wcs → resample2 → writer → verify`。
- `lib/phase3_session/`：p3_session 组装 properties/WCS/采样/原子写（p3_session.cpp 注释
  明示 P3-001..P3-004 组装）。
- `lib/phase3_session/p3_resample.{h,cpp}`：nearest / bilinear；**无 SIN/ZEA/CAR/AIT，
  无 healpix_interp4**（见 SCIENCE_OVERVIEW §4，NOT_VERIFIED 不冒充）。
- Phase3 writer 现走 CFITSIO 原子写（p3_output.cpp），**未接入 IO-001 流式 FITS**。
- `module_ports.registry.json`：`astrocs.phase3.{properties,wcs,resample2,writer,verify}`。

状态：TAN 路径声明与实现在位（PASS）；SIN/ZEA/CAR/AIT、healpix_interp4、流式 FITS
接入未实现/未验证（NOT_VERIFIED）；「投影/重采样/写出为独立节点、非重复调用
完整 p3_session_run」未达成（约束 §F.1 缺口，同 §2 注，W3/W4 范围）。

## 5. 执行与运行图

- `astrocs run/phaseN run` 均经 `cli/runtime_client.cpp run_pipeline()` →
  `astrocs_core` Runtime（`include/astrocs/core/runtime.h`、`lib/core/src/runtime.cpp`）
  单共享 executor + ThreadBudget（约束 §D.3；RT-001 合同）。
- IR 形态：`astrocs.pipeline/v1` JSON；每次 run 应产出 run-plan/graph/trace 等
  （03_TARGET_PRODUCT_AND_ARCHITECTURE.md §6）。运行图静态/观测产物生成是否完整
  属 W5/LNX 域，不在本任务复跑。

## 6. 与发布形态的关系

- 用户命令面以 `docs/api/CLI_PROTOCOL_V1.md`（API-CLI-001 冻结）为准：
  `phase1/2/3 run`、`verify`、`benchmark cpu`、`doctor` 等。
- 目标发布安装树（03 §4）：`astrocs.exe` + runtime/io/科学模块/provider DLL；
  HiPS Browser、ACR/CUDA 不入 product manifest。当前根 CMake（BLD-002）唯一
  `add_executable(astrocs)` 显式链接各静态库；**module DLL 化与发布安装树尚未完成**
  （`docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv` 等为 GENERATED 清单，
  见 ARCHITECTURE_OVERVIEW）。

## 7. 管线状态汇总

```text
三 Phase 独立命令:          PASS（phase1/2/3 run 在位）
遗留 run --phases 连跑:      FAIL（冲突未删，W4 遗留）— 已在 REVIEW 如实登记
跨 Phase 仅磁盘交换合同:      PASS（DATA-002 冻结）
RT-001 类型化 DAG 跨 Phase 拒绝: PASS
单 Phase 内部链声明与源码:    PASS（P1/P2/P3 各自链在位，静态可核）
每节点唯一 operation 绑定:   进行中/未达成（module_adapters 委托同一 session；W3/W4）
单 Phase 执行验收（当前提交复跑）: NOT_VERIFIED
Phase3 扩展（SIN/ZEA/CAR/AIT、interp4、流式 FITS 接入）: NOT_VERIFIED
```

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
