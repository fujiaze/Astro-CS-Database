# PENDING — 迁移登记（INT-001）

> 依据：`ASTROCS_DESIGN.md` §7.1（顶层结构唯一：`lib/infrastructure/observability`）；
> `cmake/ARCH-001-migration-manifest.md` §4「仍待 INT-001 处理」——
> `infrastructure/scheduler` 内含 `logging.cpp` 等，`scheduler` / `pipeline` / `observability` 三分属架构性边界决策。
> **本文件不作模块状态声明**：模块状态一律由 `tools/quality/check_module_map.py` 现场计算
> （`ASTROCS_DESIGN.md` §11.4 状态词表），禁止在登记面自证。

- 目录现状（**如实登记**，2026-09-20 实测 `git ls-files lib/infrastructure/observability | wc -l` = **13**，目录**非空**）：
  - `logging/`（3）：`log_event.py`、`log_event_v1.schema.json`、`README.md`；
  - `monitoring/`（6）：`monitor.py`、`runner.py`、`linux_procfs.py`、`windows_pdh_etw.py`（PDH/ETW 采集未实现，文件内自述）、`trace_feed.py`、`__init__.py`；
  - `probes/`（3）：`include/astrocs/probe.h`、`src/probe.cpp`、`README.md`。
- 待办（INT-001）：`lib/infrastructure/scheduler` 内 logging/监控实现按 §7.1 三分迁入本目录。
- 已知缺口：本目录**缺** `README.md` 与 `module.yaml`，而 `docs/modules/MODULE_MAP.yaml` 已声明 `readme: lib/infrastructure/observability/README.md` ⇒ 该声明当前不解析（S09 类缺口，登记不改）。
- 权威清单：`cmake/ARCH-001-migration-manifest.md` §4。
