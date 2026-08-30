# G6 Phase3 Gate Checklist

状态: **PASS** (8/8) — HEAD=`bfb0728`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | prototype退出生产注册 | PASS | P3-001 `be2d6c0`: 代码扫描 0 prototype; registry 只注册正式模块 |
| 2 | SCI/ALG/DATA先于实现 | PASS | P2-001 映射表(8 模块/7 SCI); INDEX.yaml 无 prototype ACTIVE |
| 3 | BUNIT/provenance/output path无硬编码 | PASS | P3-005 `c9a22b5`: BUNIT/RUNID/SWVER 来自 prov; 版本单源 0.10.0-alpha.1 |
| 4 | WCS plan/overflow/尺寸校验 | PASS | P3-002 `a994fe1`: uint64 溢出 guard + ASTROCS_P3_MAX_SIDE 配置合同 |
| 5 | tile并行；生产无二维串行主循环 | PASS | P3-003 `e4275ec`: tile 独立 buffer 无共享像素写 |
| 6 | constant/analytic/impulse/wrap/pole/coverage tests | PASS | P3-003 (6 fixtures) + P3-004 (6 fixtures) + P1-006 (RA wrap roundtrip) |
| 7 | FITS reopen/header/WCS/units verify | PASS | P3-005: 重开验证 dims/WCS/BUNIT/checksum/mask (修复 reopen_ok+sha256) |
| 8 | 资源门通过后才 IMPLEMENTED | PASS | P3-006 `bfb0728` 全链组装 + P2-008/P1-009 资源门覆盖 |

## 验证命令 (全部 exit 0)
- `python3 tools/check_p3_status.py` → P3-001_PASS
- `make p3_wcs_test && ./tests/unit/p3_wcs_test` → P3-002 PASS
- `make p3_interp_test && ./tests/unit/p3_interp_test` → P3-003 PASS
- `make p3_coverage_test && ./tests/unit/p3_coverage_test` → P3-004 PASS
- `make p3_output_test && ./tests/unit/p3_output_test` → P3-005 PASS
- `make p3_assembly_test && ./tests/unit/p3_assembly_test` → P3-006 PASS

## Gate 判定
G6 PASS (8/8)。进入 G7 (唯一生产路径: LEG/CLI)。
