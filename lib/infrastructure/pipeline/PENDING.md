# PENDING — 迁移登记（INT-001）

> 依据：`ASTROCS_DESIGN.md` §7.1（顶层结构唯一：`lib/infrastructure/pipeline`；禁第二名字 `runtime`）；
> `eng/cmake/ARCH-001-migration-manifest.md` §4「仍待 INT-001 处理」——
> `infrastructure/scheduler` 内含 `pipeline.cpp` / `artifact*.cpp`，三分属架构性边界决策。
> **本文件不作模块状态声明**：模块状态一律由 `eng/tools/quality/check_module_map.py` 现场计算
> （`ASTROCS_DESIGN.md` §12.5 状态词表），禁止在登记面自证。

- 目录现状（**如实登记**，2026-09-20 实测 `git ls-files lib/infrastructure/pipeline | wc -l` = **62**，目录**非空**）：
  - `orchestrator/`（50）：`cpp/`（lib/include/src/tests + CMakeLists/Makefile）、`configs/`（stage1 schema/模板/三份 panel 配置）、`README.md`；§7.1 退役计划内（见 `docs/modules/orchestrator.md`）；
  - `module_loader/`（5）：`module_registry.c/.h`、`secure_loader.c/.h`、`README.md`；
  - 阶段内内存块管线合同与工具（7）：`typed_dag.py`、`typed_dag.schema.json`、`typed_dag_contract.h`、`module_ports.registry.json`、`trace_replay.py`、`fixtures/phase2_typed_dag.json`、本文件。
- 待办（INT-001）：`lib/infrastructure/scheduler` 内 pipeline/artifact 实现按 §7.1 迁入本目录。
- 已知缺口：本目录**缺** `README.md` 与 `module.yaml`（`docs/modules/MODULE_MAP.yaml` 无 `pipeline` 条目 ⇒ S09 类缺口，登记不改）。
- 权威清单：`eng/cmake/ARCH-001-migration-manifest.md` §4。
