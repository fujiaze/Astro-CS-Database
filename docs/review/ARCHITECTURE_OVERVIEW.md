# ARCHITECTURE_OVERVIEW.md — AstroCS 架构总览（L0 负责人层）

> 目标版本: 0.10.0-alpha.2  V6 重构
> 权威: `docs/contracts/ARCH-001.md`、根 `CMakeLists.txt` (显式 target 图)、`docs/refactor/CLI_COMMAND_LAYER.md`

## 1. 分层
```
CLI (kRules 命令层) → session facade (p1/p2/p3) → 模块库 (core/io/cpu/phase1..3)
                         ↓ 委托 (不复制算法)
                    AST API / 控制合同
```

## 2. 关键架构决策 (BLD-001 冻结)
- 11 个显式 CMake target (contracts/common/core/cfitsio/aio/hips/calibration/phase2/drizzle/cpu/io + astrocs EXEC)。
- cfitsio 60 源显式清单 (禁 GLOB)。
- 版本单源 VERSION → 0.10.0-alpha.2。
- CLI 不直连科学内核 (G7 gate)。
- 生产仅纯 CPU; ACR dormant (LEG-004)。

## 3. 合同层次 (DOC-001)
- SCI (科学) → ALG (算法) → ARCH → API → CODE → TEST (六层追溯, TRACE-001)。
- 唯一状态源: TASK_LEDGER.csv。

## 4. 退出路径 (G7)
Orchestrator / AIO PipelineEngine 调度 / old Stage2 / CLI 直连 Drizzle / ACR — 全部退出或 dormant。
