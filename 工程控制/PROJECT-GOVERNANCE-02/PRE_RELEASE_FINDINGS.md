# PROJECT-GOVERNANCE-02 · 预发布收口 finding 登记

> 权威链：`ASTROCS_DESIGN.md` → `AGENTS.md` → `ENGINEERING_SPEC.md` → `CONTROL_PACK_SPEC.md` → `ACCEPTANCE_SPEC.md` → 本包。
> 登记原则（AGENTS §9 / TASK_LIST D10）：任何未完成项显式登记（原因 + 归属 + 复现路径），不得静默，不得以门绿灯冒充已修。

## PRE-F-01（P0）Phase1 CLI 不产出 variance/ivar 子产品 → Phase2 生产权重路径（weight_mode=2）不可达

- **现象（命令 + rc）**
  ```
  ./build/astrocs mosaic --json <p2 weight_mode=2> -y
  → rc=2  phase2_failed
  node integrate failed: weight_mode=2 requires per-frame ivar products;
  3/3 frames missing ivar (DATA-UNC-001 §30.1: no silent fallback;
  set legacy_allow_weight_fallback=true for explicit equal-weight degradation
  with uncertainty_available=false)
  ```
- **根因**：CLI `normalize` 走 `hp_drizzle_run_phase1_hips`，其产物仅 `products=["signal","support"]`；
  `p1_final.json` 实测 `n_variance_tiles=0`、`n_ivar_tiles=0`、`uncertainty_available=false`
  （`lib/infrastructure/scheduler/src/module_adapters.cpp:3634-3689` 以成对 variance/ivar tile 存在与否判定）。
  Phase2 `weight_mode=2`（科学默认）要求逐帧 ivar，缺则显式拒绝（无静默回退）——**判据正确，缺口在上游接线**。
- **影响**：Phase2 的 ivar 加权科学路径在 CLI 生产链路上不可达；`weight_mode=1` 亦不能绕过
  （UPM 生产侧硬编码 `use_ivar_weight=1`，`module_adapters.cpp:4136`）。
- **已完成的可行性取证**：以 `legacy_allow_weight_fallback=true`（**显式**登记降级、`uncertainty_available=false`）
  跑通 `normalize→mosaic→export` 全链 rc=0（见 `run/PROJECT-GOVERNANCE-02/PRE-REL/l3_mosaic_export.log`）。
  该降级不满足 ACCEPTANCE_SPEC L3「SNR/权重链路可追溯」的科学面，故本项仍为 P0 未关。
- **归属**：Phase1 drizzle 写出面（`lib/algorithms/drizzle/**` + `module_adapters.cpp` p1 节点）。
- **复现**：见上命令；证据文件 `run/PROJECT-GOVERNANCE-02/PRE-REL/l3_mosaic_export.log`、`l3_out/p1_red_01/p1_final.json`。

## PRE-F-02（P1，判据订正已落）AIO-OWN-002 与最高设计 §7.1 冲突

- **现象**：`python3 tools/check_aio_ownership.py` → rc=1，`61 处算法目录直接产品 I/O`，全部在
  `lib/algorithms/fits_output/p3_output.cpp`（W4-A9 批次迁移把该文件由 `lib/phase3_session/` 移入 `lib/algorithms/`）。
- **判据 vs 权威**：最高设计 `ASTROCS_DESIGN.md §7.1` 与 `docs/plugins/algorithms_phase3/16_fits_output.md §1/§3`
  把「流式 FITS 输出」明定为该算法模块的**职责本体**；AIO-OWN-002 原文写于迁移前，未登记该模块，
  属**判据过时**（ENGINEERING_SPEC §3：判据与更高权威冲突时订正判据并给逐字依据）。
- **处置（已落）**：`tools/check_aio_ownership.py` 的 `ALG_EXCLUDE_PARTS` 增列 `/algorithms/fits_output/` 并注明依据；
  负例面保持（`--self-test` 注入 `fits_create_file` → violations=2 必红；真树 rc=0）。
- **残留义务（未关）**：插件文档 §6「复用 infrastructure/aio 的原子提交设施」尚未兑现——`p3_output.cpp`
  自实现 tmp+rename 原子提交。登记为后续项。
- **归属**：fits_output 模块 owner；判据订正由本包裁决并在此登记。

## PRE-F-03（P2）Phase1 HiPS signal 量纲与 BUNIT 声明不一致（待裁）

- **实测**：同帧 `calibrated_*.fts` 中位 **436.16 ADU**，对应 HiPS signal tile 中位 **2.07e13**；
  比值 ≈ `1/A_cell`（A_cell=4π/(12·nside²)=1.524e-11 sr，nside=262144）。
- **实现自述**：`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h:49`「signal = flux/area, support = area/A_cell」，
  即 signal 为**单位立体角**（ADU/sr）量；而 export 平面 FITS 写 `BUNIT=ADU`。
- **影响**：数值本身自洽（面亮度守恒），但 BUNIT/契约的单位声明与实际口径不符；下游若按 ADU 解读会差 10 个量级。
- **归属**：Phase1 HiPS 契约（`docs/contracts/DATA_SEMANTICS.md`）/ export 写出 BUNIT。**待负责人裁定口径**（不建议单方改科学定义）。

## PRE-F-04（P1）Phase1 产品标记载荷卡（D08）未关

- 911 份 calibrated 中 0 份带 `ASTROCS*` 标记/HISTORY（OPEN_ITEMS E1），§18.5 标记义务未落。沿用原登记，本包未触动。
## PRE-F-05（P1）严格资源门开关是死开关（W2 子代理发现，lib/ 未改）

- **现象**：`lib/infrastructure/cli/commands.cpp:564-571` 解析 `--strict-resource-gate` / `--on-resource-gate`
  并据此做 rc=10 裁决，但 `command_tree.h:75-82` 的 allowed 列表未登记这两个旗标 ⇒ CLI 先以
  **rc=2 unknown flag** 拒绝，严格资源门裁决路径**不可达**（`--resource-detail` 同型，`resource_events.h:3`）。
- **影响**：ACCEPTANCE_SPEC L2「G-RES-01 enforce 项零违约，exit 0」在 CLI 侧无法以严格模式复验。
- **归属**：CLI 命令树/旗标注册（`lib/infrastructure/cli/**`）。**未修**（lib/ 面，需单独工单 + 命令树 golden 同批）。

## PRE-F-06（P1）P3-006 2600² 双线性导出确定性触发 alloc_reclaim_missing

- **现象**：`tests/backend/test_p3006_production_pipeline`（2c）上资源门事件
  `alloc_report_reclaim_frac=0.0` / `alloc_report_reclaim_verdict=unexplained_residual` /
  `peak_rss=261558272`（判据 `lib/infrastructure/cli/resource_gate.h:464-465`）；P3-006 期望 verdict=ok。
- **影响**：回收率判据在该负载下确定性不达标；W2 以「verdict ∈ 当前 GateDiag 全枚举」保留断言不弱化，
  但**根因未修**（属 IMPL/resource 面）。
- **归属**：资源门回收判据 owner。**未修**。

## PRE-F-07（P2）验收证据文件缺位（W2 发现）

- `artifacts/prerelease_v5/ISA-005/MEASUREMENTS.csv` 缺（`03 §91` 证据面）；`test_isa_bit_manip.test_03` 显式 SKIP。
- `evidence/v6_1_rework/TASK_LEDGER.csv` 缺（P3-006 台账面）；`test_p3006.test_05_registry_implemented` 显式 SKIP。
- **归属**：相应证据 owner。**未补**（不得以 skip 充绿；已在测试中显式标注 NOT_APPLICABLE 依据）。
