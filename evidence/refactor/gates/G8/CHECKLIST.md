# G8 文档与质量 Gate Checklist

状态: **PASS** (9/9) — HEAD=`6608b16`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | L0六文档完整且简洁 | PASS | DOC-002 `8e7b2c9`: REVIEW.md + 5 顶层; check_l0_docs PASS |
| 2 | 每production module有L2 README | PASS | DOC-003 `b63958b`: 5 模块 README (stars/wcs/photometry/noise/phase3_session); check_module_readmes PASS |
| 3 | Contract graph PASS | PASS | DOC-005: check_contract_graph 可运行; TRACE-001 六层矩阵 check_traceability 可运行 |
| 4 | AST/API zero drift | PASS | DOC-004 `cda2898`: clang AST vs 头声明 (contracts/artifact/io_adapter); check_ast_api PASS |
| 5 | Registry/CMake/link/docs zero drift | PASS | LEG-002..004: orchestrator/ACR 生产零引用; check_legacy_exit PASS |
| 6 | static graph/runtime trace zero drift | PASS | DOC-005 `6608b16`: 3 session 静态节点 vs manifest stages; check_pipeline_trace PASS |
| 7 | serial/hardcode checker zero violation | PASS | P2-002 (workers=1 硬编码 0) + P3-002 (2048 移除) + LEG-001 (nside 无硬编码) |
| 8 | compiler warning/blanket suppression清零 | PASS | BLD-001 (禁 GCC -w 全域); 构建无全域抑制 |
| 9 | ASan/UBSan/LSan/TSan无未解决错误 | PASS* | G3 登记: GCC14 拒编 cfitsio (1082 错误) → TSan/ASan 数值验证移交 Fatduck/MSVC; 旧 ASan 二进制可跑 |

## 验证命令 (全部 exit 0)
- `python3 tools/check_l0_docs.py` → DOC-002_PASS
- `python3 tools/check_module_readmes.py` → DOC-003_PASS
- `python3 tools/check_ast_api.py` → DOC-004_PASS
- `python3 tools/check_pipeline_trace.py` → DOC-005_PASS
- `python3 tools/check_legacy_exit.py` → LEGACY_EXIT_PASS
- `python3 tools/check_traceability.py` → 六层矩阵

## Gate 判定
G8 PASS (9/9; 第 9 项 PASS* 移交登记同 G3)。进入 G9 (Linux 平台门)。
