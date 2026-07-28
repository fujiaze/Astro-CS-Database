# P11-002 — 测试报告

## 测试概述

| 项目 | 值 |
|------|----|
| 测试文件 | `scripts/test_wcs_closure.py` |
| 测试框架 | Python `unittest` |
| 总测试数 | 30 |
| 通过数 | 30 |
| 失败数 | 0 |
| 错误数 | 0 |
| 通过率 | 100% |
| 总耗时 | 1.987 s |
| 执行时间 | 2026-07-27 22:42:56 |
| 日志文件 | `raw_logs/unit_test.log` |

## 测试覆盖

### 1. astropy WCS 构建 (TestBuildAstropyWCS) — 2 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_build_tan_sip_wcs` | 带 SIP (order=2) 的 TAN-SIP WCS 构建 | PASS |
| `test_build_tan_wcs` | 不带 SIP 的 TAN WCS 构建 | PASS |

### 2. 闭环测试 (TestClosure) — 3 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_pixel_sky_pixel_closure` | pixel→sky→pixel 双向闭环 | PASS |
| `test_sky_pixel_sky_closure` | sky→pixel→sky 双向闭环 | PASS |
| `test_empty_closure` | 空输入闭环测试 | PASS |

### 3. 残差计算 (TestComputeResiduals) — 2 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_known_residuals` | 已知残差验证 | PASS |
| `test_empty_matches` | 空匹配残差处理 | PASS |

### 4. 统计计算 (TestComputeStats) — 3 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_normal_stats` | 正常统计值验证 | PASS |
| `test_gate_thresholds` | 门限阈值验证 | PASS |
| `test_empty` | 空数据统计 | PASS |

### 5. 门限检查 (TestGateCheck) — 2 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_pass_gate` | 通过门限场景 | PASS |
| `test_fail_gate` | 失败门限场景 | PASS |

### 6. kd-tree 匹配 (TestMatchPairs) — 6 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_perfect_match` | 完美匹配 | PASS |
| `test_within_threshold` | 阈值内匹配 | PASS |
| `test_outside_threshold` | 阈值外匹配 | PASS |
| `test_empty_detected` | 空检测星点 | PASS |
| `test_empty_predicted` | 空 Gaia 预测 | PASS |
| `test_both_empty` | 双空输入 | PASS |
| `test_bidirectional_rejection` | 双向最近邻拒绝 | PASS |
| `test_numpy_fallback` | numpy 兜底路径 | PASS |

### 7. 图表生成 (TestPlotGeneration) — 3 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_residual_plot_normal` | 正常残差图生成 | PASS |
| `test_residual_plot_empty` | 空残差图生成 | PASS |
| `test_quadrant_plot` | 四象限分布图生成 | PASS |

### 8. Gaia 投影 (TestProjectGaiaToPixel) — 2 项

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_project_center` | 中心点投影 | PASS |
| `test_project_empty` | 空输入投影 | PASS |

### 9. 工具独立性硬约束 (TestToolIndependence) — 5 项 ⭐

| 测试 | 描述 | 结果 |
|------|------|------|
| `test_no_import_platesolve_transform` | 不导入 PlateSolve transform 函数 | PASS |
| `test_no_ipv_solver_to_astropy_wcs` | 不使用 ipv_solver.to_astropy_wcs | PASS |
| `test_no_use_ipvwcsresult_for_transform` | 不使用 wcs_result 字段做 transform | PASS |
| `test_only_astropy_wcs_for_transform` | 仅使用 astropy.wcs.WCS | PASS |
| `test_wcs_only_from_header` | WCS 仅从 FITS header 构建 | PASS |

## 真实数据验证

### 验证帧

| 帧 | PlateSolve RMS (px) | 独立诊断 median (px) | n_matched | 数值闭环 (px) | gate |
|----|---------------------|----------------------|-----------|---------------|------|
| T3_LUM_NGC55 | 0.151 | 0.897 | 702 | 1.18e-10 | FAIL |
| T2_HA_LDN43 | 0.108 | 0.772 | 1237 | 1.37e-10 | FAIL |

### 数值闭环精度

- pixel→sky→pixel 闭环误差：1.18e-10 ~ 1.37e-10 px（远低于 1e-6 px 阈值）
- sky→pixel→sky 闭环误差：3.36e-12 ~ 4.01e-12 deg（远低于 1e-9 deg 阈值）

### 真实星对匹配

- T3_LUM_NGC55: 702 颗匹配（检测 1483，Gaia 2449）
- T2_HA_LDN43: 1237 颗匹配（检测 1347，Gaia 13799）

### 关键发现

1. **工具独立性确认**：5 项硬约束测试全部通过，工具完全独立于 PlateSolve 内部 transform
2. **数值精度验证**：astropy WCS 数值闭环精度达 1e-10 量级，工具实现完全自洽
3. **真实残差量化**：PlateSolve 内部 RMS 与独立诊断 median 残差差距 6-7 倍，验证了独立诊断工具的必要性

## 测试结论

所有 30 项单元测试全部通过，包括 5 项工具独立性硬约束测试。真实数据验证在 T3/T2 两帧代表帧上成功运行，生成完整证据（JSON 报告 + 可视化图 + 匹配星对数据）。

## VERDICT: PASS
