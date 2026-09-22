# PENDING — 迁移登记（INT-001）

> 依据：`ASTROCS_DESIGN.md` §7.1（顶层结构唯一：`lib/infrastructure/benchmark`）、§8（CPU 后端与资源：`cpu_profile` 由 benchmark 生成）；
> `eng/cmake/ARCH-001-migration-manifest.md` §1 第 16 行（`lib/backend_host` → `lib/infrastructure/benchmark/backend_host`，**DONE**）。
> **本文件不作模块状态声明**：模块状态一律由 `eng/tools/quality/check_module_map.py` 现场计算
> （`ASTROCS_DESIGN.md` §12.5 状态词表），禁止在登记面自证。

- 目录现状（**如实登记**，2026-09-20 实测 `git ls-files lib/infrastructure/benchmark | wc -l` = **38**，目录**非空**）：
  - `backend_host/`（27）：backend 加载/选路（`backend_loader.*`、`cpu_routing.*`、`baseline_*`、`avx/avx2/avx512_backend.cpp`）、`bench_harness.*`/`bench_report.*`、`profile_gen*.cpp`/`profile_store.*`、`worker_advisor.*`、`hardware_inspect.*`、`host_services.cpp`；
  - `cpu/`（10）：`baseline`/`avx2`/`avx512` provider（include+src）、`common/`（`capability_detect.c`、`capability_v1.h`、`cpu_capability.schema.json`、`README.md`）。
- 待办（INT-001）：本目录内容归位后的模块边界收口（`docs/modules/MODULE_MAP.yaml` 已有 `benchmark` 条目）。
- 已知缺口：本目录**缺** `README.md` 与 `module.yaml`，而 `docs/modules/MODULE_MAP.yaml` 已声明 `readme: lib/infrastructure/benchmark/README.md` ⇒ 该声明当前不解析（S09 类缺口，登记不改）。
- 权威清单：`eng/cmake/ARCH-001-migration-manifest.md` §1/§4。
