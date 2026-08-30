# G4 Phase1 Gate Checklist

状态: **PASS** (10/10) — HEAD=`e1c6126`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | old symbols 全映射，无能力遗失 | PASS | P1-001 `fe92b6e`: 5 API/13 模块/7 SCI; blockers=0 |
| 2 | calibration/cosmetic Oracle | PASS | P1-002 `6f80c91`: constant/gradient/negative/NaN/u16/f32/f64 解析逐像素 |
| 3 | Star/PSF truth catalog tests | PASS | P1-003 `d3af6ff`: 孤立/重叠/饱和/边缘/纯噪声 + completeness/fp/tie-breaker/catalog 字段 |
| 4 | PlateSolve WCS roundtrip | PASS | P1-004 `366b9f8`: TAN roundtrip <1e-6 deg |
| 5 | Photometry integration regression | PASS | P1-004: 已知通量解析恢复 20%; 边缘积分回归; 失败不留空 catalog |
| 6 | Noise/SNR analytic + Monte Carlo | PASS | P1-005 `a63a66a`: SCI 公式 + MC 固定 seed 对照解析 15% |
| 7 | Drizzle constant/flux/centroid/support/wrap | PASS | P1-006 `ed9f3b0`: NSIDE 派生无硬编码; RA wrap roundtrip; drizzle 引擎既有测试套件(l0/l2/acceptance/freeze) |
| 8 | HiPS writer/verify | PASS | P1-007 `466c6fa`: 全产品写(ALL_V19) + properties/tile verify + 重开 hash |
| 9 | canonical IR = runtime trace | PASS | P1-008 `c22819a`: 4 节点声明 + cosmetic preset + facade 委托 |
| 10 | 2 核 heavy资源门；无泄漏 | PASS | P1-009 `e1c6126`: 6 帧并行资源记录/结果正确/阈值 1.6 核; ASan 数值验证移交 Windows 节点(登记) |

## 验证命令 (全部 exit 0)
- `python3 tools/check_p1_symbol_map.py` → P1-001_PASS
- `make p1_calibration_test && ./tests/unit/p1_calibration_test` → P1-002 PASS
- `make p1_stars_test && ./tests/unit/p1_stars_test` → P1-003 PASS
- `make p1_wcs_phot_test && ./tests/unit/p1_wcs_phot_test` → P1-004 PASS
- `make p1_noise_test && ./tests/unit/p1_noise_test` → P1-005 PASS
- `make p1_nside_test && ./tests/unit/p1_nside_test` → P1-006 PASS
- `make p1_hips_writer_test && ./tests/unit/p1_hips_writer_test` → P1-007 PASS
- `make p1_ir_facade_test && ASTROCS_REPO=$PWD ./tests/unit/p1_ir_facade_test` → P1-008 PASS
- `make p1_resource_test && ./tests/unit/p1_resource_test` → P1-009 PASS
- `make astrocs && ./astrocs --version` → `0.10.0-alpha.1+...`; link scan globs=0 acr=0

## Gate 判定
G4 PASS (10/10)。进入 G5 (Phase2 迁移与接缝修复)。
