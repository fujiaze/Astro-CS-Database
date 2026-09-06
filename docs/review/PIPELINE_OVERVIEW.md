# PIPELINE_OVERVIEW — L0 治理评审层（管线）

> 文档 ID：DOC-REVIEW-PIPELINE-001
> 状态：ACTIVE_INFORMATIVE（L0 治理评审汇总层）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源）
> 权威：`AstroCS_ENGINEERING_CONSTRAINTS.md` §A、`docs/api/CLI_PROTOCOL_V1.md`（API-CLI-001）、
> `docs/contracts/{ARCH-001,RT-001,DATA_ARTIFACTS,DATA_SEMANTICS}.md`、
> `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md`、
> L0 汇总权威 `docs/owner/PIPELINE_OVERVIEW.md`（GOV-004）。

## 1. 三 Phase 隔离模型（约束 §A）

Phase1 / Phase2 / Phase3 是三个隔离的产品命令，不是固定顺序流水线：

```text
Phase1: 单帧 light + masters/catalog/config → 单帧标准化 HiPS + manifest
Phase2: 任意一组合同兼容 HiPS → 马赛克 HiPS + UPM/rejection/integration provenance
Phase3: 任一合同兼容 HiPS（不要求来自 Phase2） → 平面 FITS + WCS/coverage/validity/provenance
```

- 阶段间只通过原子发布、哈希与 provenance 完整的磁盘产品/manifest 交换（§A.6；DATA-002）。
- 禁止同进程 `--phases 1,2,3` 连续跑（§A.4）；外部脚本可显式依次启动三个独立进程。
- Phase3 不得假设输入来自 Phase2；Phase2 不得假设输入由同一进程 Phase1 产生（§A.5）。

## 2. 命令面现状（源码在位，静态可核）

- 唯一 run 入口是独立命令 `phase1 run / phase2 run / phase3 run`
  （`cli/parser.cpp` kRules + `docs/api/CLI_PROTOCOL_V1.md` 命令树一致）。
- **CLI-002 已删除顶层连续管线**：`run --phases N` 与 `graph` 命令已从
  parser/commands 移除（`cli/commands.cpp` 注释：cmd_run_pipeline / cmd_graph
  已随 CLI-002 移除），现解析到 unknown command（exit 2）→ 约束 §A.4
  冲突项已在代码面关闭；负测断言 run/graph exit 2 在 tests 内登记。
- 每 phaseN run 走 Runtime 单 Phase IR 子图（`cli/commands.cpp`：
  RT-008 注释，phase1/2/3 各自独立 session/manifest/run ID）。
- 资源门禁已接入生产路径（MON-004）：`cli/resource_gate.h` `evaluate_gate`
  在 phase1/2/3 run 生效（`cli/commands.cpp` `run_with_resource_gate`），
  门禁失败 exit 10（RESOURCE），first-10s 快速失败与短任务豁免内建。

## 3. 内部链（源码在位）

- Phase1：`io_read → calibrate → cosmetic → io_write`
  （`lib/phase1_session/p1_session.cpp`）；模块族 `astrocs.phase1.*`
  注册于 `lib/core/src/module_adapters.cpp`，端口登记于
  `runtime/pipeline/module_ports.registry.json`。
- Phase2：`coverage → sample → upm → reject → integrate → write`
  （`lib/phase2_session/p2_session.cpp`、`lib/phase2/src/*.cpp`）。
- Phase3：TAN 投影 + nearest/bilinear 重采样 + FITS 原子写
  （`lib/phase3_session/{p3_session,p3_wcs,p3_resample,p3_output}.cpp`）；
  SIN/ZEA/CAR/AIT、`healpix_interp4` 未实现，不宣称。

## 4. 已知缺口（如实记录）

1. 约束 §F.1「每 DAG 节点唯一真实模块 operation」未完全达成：
   `lib/core/src/module_adapters.cpp` 中 IR 子模块 factory 仍委托同一
   phaseN session（W3 模块化范围），不宣称已实现。
2. 单 Phase 执行验收（当前提交复跑）NOT_VERIFIED；历史存档不冒充。
3. Phase3 流式 FITS 输出接入：IO-001 接口在位，p3 writer 仍走 CFITSIO 原子写。

---
authoring_task: DOC-L0
authoring_layer: docs/review (L0 governance review)
base_product_version: 0.11.0-alpha.2
