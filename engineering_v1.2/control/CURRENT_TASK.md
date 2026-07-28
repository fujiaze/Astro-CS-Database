# 当前任务

`P11-004`：在 WCS 生产端实施统一修正（基于 P11-003 根因结论）。

## P11-003 已完成（2026-07-28）

- 任务目标：在 T1-T4 代表帧复现 WCS 闭环缺陷，量化 X/Y 偏差、象限、SIP 阶数
- 工具：P11-003 v1.0（subset driver + astropy WCS 独立诊断）
- 执行范围：16 帧代表帧
  - T2: 5 帧（LDN43 ×4 + NGC1727 ×1）
  - T3: 6 帧（NGC55 ×6）
  - T4: 5 帧（Galaxy_Center ×5）
- 执行方式：并行 Subagent（Group A + Group B 各 4 帧）+ 3 帧重跑（NTFS 压缩损坏恢复）
- 关键结果：

| 维度 | 结果 |
|------|------|
| 求解成功率 | 16/16 = 100% |
| 诊断成功率 | 16/16 = 100% |
| gate 通过率 | 8/16 = 50% |
| median solve RMS | 0.1146 px |
| median 残差中位数 | 0.736 px |

- 跨帧模式（根因结论）：

| 因素 | 模式 |
|------|------|
| 焦距/FOV（主导） | T4 (200mm 大 FOV) 100% 通过；T2/T3 (1900mm 小 FOV) 27% 通过 |
| 滤镜类型（次要） | 窄带 HA/OIII 66.7% 通过；宽带 RGB/LUM 10% 通过 |
| Y 方向系统偏差 | 15/16 帧 Y 残差 > X 残差（93.75%） |
| SIP 写入/解析损失 | 已排除（PS↔Sky 闭环 1e-10 px） |

- VERDICT: PARTIAL
  - WCS 闭环实现正确（数值闭环精度 1e-10 px，非代码缺陷）
  - gate 失败根因：SIP order=3 在小 FOV 下建模不足 + Y 方向系统偏差 + 宽带暗星质心精度
- 证据：`engineering_v1.2/evidence/P11-003/`
  - 4 份标准报告：TASK_REPORT.md / TEST_REPORT.md / EVIDENCE_INDEX.md / REVIEW_REPORT.md
  - 16 帧 closure_report.json + matched_pairs.json + residual_plot.png + quadrant_plot.png
  - p11_003_summary.json（16 帧汇总 + 跨帧模式 + 根因结论）
  - group_a_summary.json + group_b_summary.json + rerun_corrupted_summary.json
  - REPRESENTATIVE_FRAMES_ARCHIVE.json（16 帧设备档案 + 执行状态）
  - raw_logs/（执行日志）

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
- P11-003：T1-T4 代表帧 WCS 闭环缺陷复现（16 帧，8/16 gate 通过，VERDICT: PARTIAL）

## 下一步：P11-004

依据 `tasks/P11-004.md`：

- 在 WCS 生产端实施统一修正（基于 P11-003 根因结论）
- 依赖：P11-003（已满足，根因结论已就绪）
- 待修正根因：
  1. SIP order=3 在小 FOV (T2/T3 1900mm) 下建模不足 → 考虑提高 SIP order 或引入更高阶畸变模型
  2. Y 方向系统偏差（15/16 帧 Y > X）→ 排查检测器/光学/坐标变换链中的 Y 方向系统误差
  3. 宽带暗星质心精度 → 考虑质心算法优化或 SNR 加权
- 不修正项：
  - 大 FOV (T4 200mm) 已 100% 通过，无需修正
  - 窄带 HA/OIII 66.7% 通过，质心精度已满足
- 验证：修正后重跑 16 帧代表帧，gate 通过率目标 ≥ 14/16 (87.5%)

## 已知 BLOCKED 项

- 123 Lum Light 帧（T2 25 + T4 98）缺 Lum Flat Master，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录。
