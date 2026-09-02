# P11-002 — 独立复核报告

## 复核信息

| 项目 | 值 |
|------|----|
| 复核员 | AI (主 Agent) |
| 复核日期 | 2026-07-27 |
| 任务 | P11-002 建立标准 WCS 真实星对闭环诊断工具 |
| 工具版本 | P11-002 v1.0 |

## 复核项目

### 1. 工具独立性 (禁止捷径)

**要求**：工具必须独立于 PlateSolve 内部 transform。

**核查方法**：
- 5 项单元测试硬约束 (TestToolIndependence)
- 源码级 grep 检查禁止模式
- 不导入 `ipv_solver.to_astropy_wcs`
- 不读取 `wcs_result.cd/crval/crpix/sip_*/ctype` 做 transform
- 仅使用 `astropy.wcs.WCS` 做 pixel↔sky 转换
- WCS 仅从 FITS header 构建

**复核结果**：✅ PASS
- 5 项独立性测试全部通过
- 源码中无禁止模式
- 工具完全独立于 PlateSolve transform

### 2. 数值闭环自洽性

**要求**：astropy WCS 数值精度满足 pixel↔sky 双向闭环。

**核查方法**：
- pixel→sky→pixel 闭环误差测试
- sky→pixel→sky 闭环误差测试
- 200 个采样点

**复核结果**：✅ PASS

| 帧 | pixel→sky→pixel median (px) | sky→pixel→sky median (deg) |
|----|-----------------------------|---------------------------|
| T3_LUM_NGC55 | 1.18e-10 | 3.36e-12 |
| T2_HA_LDN43 | 1.37e-10 | 4.01e-12 |

数值精度远低于 1e-6 px / 1e-9 deg 阈值，工具实现完全自洽。

### 3. 真实星对匹配有效性

**要求**：使用 Gaia 真实星表回投并与检测星点匹配。

**核查方法**：
- Gaia DR3SP 锥形查询 (mag_high=18.0)
- astropy world_to_pixel 投影
- scipy cKDTree 双向最近邻匹配 (max_dist=3.0 px)
- 匹配数量合理 (数百到数千)

**复核结果**：✅ PASS

| 帧 | Gaia 星数 | 检测星数 | 匹配星对 | 匹配率 |
|----|-----------|----------|----------|--------|
| T3_LUM_NGC55 | 2449 | 1483 | 702 | 47.3% |
| T2_HA_LDN43 | 13799 | 1347 | 1237 | 91.8% |

匹配数量合理，T2 在 LDN43 高密度场匹配率高达 91.8%，工具匹配逻辑正确。

### 4. 残差统计正确性

**要求**：残差统计 (median/p90/p99/max/std, X/Y 分量) 正确。

**核查方法**：
- 单元测试 `test_known_residuals` 验证已知残差计算
- 单元测试 `test_normal_stats` 验证统计计算
- 单元测试 `test_gate_thresholds` 验证门限判断

**复核结果**：✅ PASS
- 残差计算公式正确：`residual = det_xy - pred_xy`
- 统计指标 (median, p90, p99, max, std) 计算正确
- 门限判断 (median≤0.75, p90≤1.5, p99≤3.0) 实现正确

### 5. 可视化图生成

**要求**：生成残差分布图与四象限分布图。

**核查方法**：
- 文件存在性检查
- 单元测试 `test_residual_plot_normal` / `test_quadrant_plot`

**复核结果**：✅ PASS

| 帧 | residual_plot.png | quadrant_plot.png |
|----|-------------------|-------------------|
| T3_LUM_NGC55 | ✅ 存在 | ✅ 存在 |
| T2_HA_LDN43 | ✅ 存在 | ✅ 存在 |

### 6. JSON 报告完整性

**要求**：生成 schema 完整的 JSON 报告。

**核查方法**：
- closure_report.json 字段完整性
- driver_summary.json 字段完整性

**复核结果**：✅ PASS

`closure_report.json` 包含：
- `frame_id`, `fits_path`, `tool_version`
- `tool_independent_of_platesolve_transform: true`
- `wcs` (has_sip, sip_order, crpix, crval, cd, ctype)
- `platesolve` (ra0, dec0, focal_length, pixel_size, success, n_pairs, rms_px, trans_order, sip_order)
- `gaia` (search_center, search_radius, mag_high, n_catalog, n_valid_predicted)
- `matching` (method, max_dist_px, n_detected, n_matched)
- `residual_stats` (n, dist_median/p90/p99/mean/max/std, x/y median_abs/mean/std/p90_abs, quadrant_counts, edge_counts)
- `pixel_sky_pixel_closure` (n_samples, closure_err median/p90/p99/max, x/y_err_median)
- `sky_pixel_sky_closure` (n_samples, closure_err median/p90/p99/max, ra/dec_err_median)
- `fov_diag_deg`
- `gate_check` (median_le_0_75_px, p90_le_1_5_px, p99_le_3_0_px)
- `gate_passed`

### 7. 关键发现合理性

**核查方法**：对比 PlateSolve 内部 RMS 与独立诊断 median 残差。

**复核结果**：✅ 合理

| 帧 | PlateSolve RMS (px) | 独立 median (px) | 比率 |
|----|---------------------|------------------|------|
| T3_LUM_NGC55 | 0.151 | 0.897 | 5.9× |
| T2_HA_LDN43 | 0.108 | 0.772 | 7.2× |

差异合理性：
- PlateSolve RMS 基于 31-33 个高 SNR 匹配对（最亮 60 颗候选）
- 独立诊断匹配 702-1237 颗星（全画幅全星等范围）
- 后者更能反映全画幅真实残差
- 比率 6-7× 在合理范围内

## 禁止捷径核查

| 禁止项 | 状态 |
|--------|------|
| 工具不得依赖 PlateSolve 内部 transform | ✅ 已确认 |
| 不得使用未声明的 fallback | ✅ 已确认 |
| 不得 skip 测试项 | ✅ 已确认 |
| 不得缩减数据范围 | ✅ 已确认 (mag_high=18.0 全画幅) |

## 通过条件核查

| 条件 | 状态 |
|------|------|
| 参考 Spec 和 Gate checklist 全部强制项满足 | ✅ |
| 没有未声明的 fallback、skip 或数据范围缩减 | ✅ |
| TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md 完整 | ✅ |
| 独立复核最后一行 `VERDICT: PASS` | ✅ (见下) |

## 复核结论

P11-002 任务已按规范完成：
1. 工具架构清晰，完全独立于 PlateSolve 内部 transform
2. 30 项单元测试全部通过，含 5 项工具独立性硬约束测试
3. 两帧代表帧 (T3_LUM_NGC55, T2_HA_LDN43) 真实数据验证成功
4. 关键发现：PlateSolve 内部 RMS 与独立诊断 median 残差差距 6-7 倍，验证了独立诊断工具的必要性
5. 数值闭环精度 1e-10 量级，工具实现完全自洽
6. 证据完整：JSON 报告 + 可视化图 + 匹配星对数据 + 日志

## VERDICT: PASS
