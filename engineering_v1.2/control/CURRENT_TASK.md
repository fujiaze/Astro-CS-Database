# 当前任务

`P11-003`：在 T1-T4 代表帧复现 WCS 闭环缺陷，量化 X/Y 偏差、象限、SIP 阶数。

## P11-002 已完成（2026-07-27）

- 工具版本：P11-002 v1.0
- 工具架构：独立于 PlateSolve 内部 transform
  - 核心模块：`scripts/wcs_closure_diagnostic.py`
  - 单元测试：`scripts/test_wcs_closure.py`（30/30 PASS，含 5 项独立性硬约束）
  - Driver 脚本：`scripts/run_diagnostic.py`
  - FITS header 工具：`scripts/check_fits_header.py`
- 工具独立性硬约束（5 项单元测试强制）：
  - 不导入 `ipv_solver.to_astropy_wcs`
  - 不读取 `wcs_result.cd/crval/crpix/sip_*/ctype` 做 transform
  - 仅用 `astropy.wcs.WCS` 做 pixel↔sky 转换
  - WCS 仅从 FITS header 构建
- 验证帧（2 帧）：
  - T3_LUM_NGC55（T3 / LUM / NGC55 / 4096×4096 / FOV_diag=1.558°）
  - T2_HA_LDN43（T2 / HA / LDN43 / 4096×4096 / FOV_diag=1.575°）
- 关键结果：

| 帧 | PlateSolve RMS (px) | n_pairs (solve) | 独立诊断 median (px) | n_matched (诊断) | gate |
|----|---------------------|-----------------|----------------------|-------------------|------|
| T3_LUM_NGC55 | 0.151 | 31 | 0.897 | 702 | FAIL |
| T2_HA_LDN43 | 0.108 | 33 | 0.772 | 1237 | FAIL |

- 数值闭环精度：1.18e-10 ~ 1.37e-10 px（astropy WCS 完全自洽）
- 关键发现：
  - PlateSolve 内部 RMS 与独立诊断 median 残差差距 6-7 倍，验证独立诊断工具必要性
  - T3 Y 方向偏差主导 (0.848 vs 0.218 px)
  - T2 X/Y 均衡偏差 (~0.5 px each)
  - Q4 象限 (+X, -Y) 星对偏多（两帧一致）
  - SIP_ORDER=3 两帧一致
- 证据：`engineering_v1.2/evidence/P11-002/`
  - 4 份报告：TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md
  - 2 帧 closure_report.json + matched_pairs.json + residual_plot.png + quadrant_plot.png
  - driver_summary.json
  - unit_test.log + run_diagnostic.log
- VERDICT: PASS

## 历史任务（已完成）

- P09-001：v1.1 基线冻结 + v1.2 开发包安装（4 件套）
- P09-002：INTERNAL_DETECTION_SHARED_EXPORT 命名统一（6/6 PASS）
- P09-003：canonical_dataset_v1.2 冻结（44 文件 SHA-256，7 测光失败帧，4 HCSD 基线）
- P10-001：TestData 目录盘点（3 设备 / 49 light 组 / 49+27 Header 采样）
- P10-002：T1-T4 设备档案建立（4 profile + summary，710 lights，76/76 PASS）
- P10-003：主校准帧盘点（27 文件 CSV + summary，20/20 PASS）
- P10-004：滤镜规范名与别名冻结（52 别名映射，23/23 PASS）
- P10-005：Light 到 Master 唯一解析（587/710 resolved，123 missing_lum_flat，23/23 PASS）
- P10-006：T1-T4 真实校准代表帧验证（16/16 PASS，25/25 测试 PASS）
- P11-001：坐标约定冻结（COORDINATE_CONVENTION.md，7 坐标系 + 22 变量，19/19 PASS）
- P11-002：WCS 真实星对闭环诊断工具建立（30/30 测试 PASS，2 帧代表帧验证，VERDICT: PASS）

## 下一步：P11-003

依据 `tasks/P11-003.md`：

- 在 T1-T4 全部代表帧复现 WCS 闭环缺陷
- 依赖：P11-002（已满足，工具已建立）
- 量化 X/Y 偏差分布、象限偏差、SIP 阶数对残差影响
- 对比不同设备/滤镜/目标的偏差模式
- 工具已就绪：可直接调用 `wcs_closure_diagnostic.diagnose_frame` 批量处理

## 已知 BLOCKED 项

- 123 Lum Light 帧（T2 25 + T4 98）缺 Lum Flat Master，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录。
