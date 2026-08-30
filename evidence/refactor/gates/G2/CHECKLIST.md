# G2 Runtime Gate Checklist

状态: **PASS** (9/9) — commit `e498e7b` + backpressure/recovery 增补, HEAD=`e498e7b`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | 显式 CMake targets；无 production GLOB | PASS | BLD-001 `338eecc`: 11 target; check_link_scan globs=0; cfitsio 生成清单 |
| 2 | ACR 默认 OFF | PASS | BLD-001: `ASTROCS_ENABLE_ACR=OFF`; 生产 CLI ACR 符号 78→0 |
| 3 | Result/error/cancel 单测 | PASS | CORE-001 `c572aa1`: core_contracts_test 7 组 |
| 4 | DataArtifact/provenance roundtrip | PASS | CORE-002 `3e98a22`: core_artifact_test 4 组 roundtrip/hash |
| 5 | Registry duplicate/ABI/contract negative tests | PASS | CORE-003 `e29aae7`: core_module_test duplicate/ABI/namespace/ports |
| 6 | Pipeline cycle/port/unit negative tests | PASS | CORE-004 `c497a28`: core_pipeline_test 7 组含控制包 fixtures |
| 7 | RunContext 无 singleton/global scheduler | PASS | CORE-005 `6389105`: RunContext 实例化; 无 singleton |
| 8 | 2 核 DAG 并发、backpressure、cancel、recovery | PASS | CORE-006 `dedbfad`: 2 核并发 max_concurrent>=2; budget=1 backpressure<=1; cancel 传播; 失败后 skip |
| 9 | JSONL trace schema/sequence | PASS | CORE-008 `e498e7b`: Logger JSONL 10 字段 + 单调 seq |

## 验证命令 (全部 exit 0)
- `make astrocs && ./astrocs --version` → `0.10.0-alpha.1+g...`
- `make core_contracts_test && ./tests/unit/core_contracts_test` → PASS
- `make core_artifact_test && ./tests/unit/core_artifact_test` → PASS
- `make core_module_test && ./tests/unit/core_module_test` → PASS
- `make core_pipeline_test && ASTROCS_REPO=$PWD ./tests/unit/core_pipeline_test` → PASS
- `make core_context_test && ./tests/unit/core_context_test` → PASS
- `make core_scheduler_test && ./tests/unit/core_scheduler_test` → PASS (8 组含 backpressure/recovery)
- `make core_checkpoint_test && ./tests/unit/core_checkpoint_test` → PASS
- `make core_logging_test && ./tests/unit/core_logging_test` → PASS
- `python3 tools/check_link_scan.py build/root-cmake/astrocs` → LINK_SCAN_PASS globs=0 acr_symbols=0

## Gate 判定
G2 PASS。进入 G3 (I/O 与 CPU)。
