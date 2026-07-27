# 当前任务

`P11-001`：冻结内部/图像/FITS/WCS坐标约定。

## P10-006 已完成（2026-07-27）

- 3 个数据交付物：
  - REPRESENTATIVE_CALIBRATION_REPORT.csv（16 行，每行一个代表帧校准结果含 Light/Bias/Dark/Flat 路径 + 校准前后统计 + 坏点数 + 通过判定）
  - CALIBRATION_VALIDATION_SUMMARY.json（汇总统计：按设备/按滤镜分布 + 通过率 + 16 个代表帧详情）
  - calibrated/*.fits（16 个校准后 FITS 文件，float32，含 CALIBRAT/SRCFRAME 关键字）
- 2 个脚本交付物：
  - validate_representative_calibration.py（校准主脚本，实现 (Light - Dark) / NormalizedFlat 公式 + Flat 归一化到 median=1.0 最小值裁剪 0.1 + 坏点检测）
  - test_calibration_outputs.py（25 项测试：contract 5 + unit 12 + e2e 4 + forbidden shortcut 4）
- 关键统计：
  - 16 个代表帧：T2:5 + T3:6 + T4:5（按滤镜：BLUE:3 + GREEN:3 + HA:3 + LUM:1 + OIII:3 + RED:3）
  - 16/16 PASS（100%），0 失败
  - 0 NaN/0 Inf/0 极端值，坏点比例 0%
  - 校准后尺寸一致性 16/16 PASS，统计量有限 16/16 PASS
- FITSWriter 方法名修复：`write_fits()` 不存在，正确方法为 `write()`
- 禁止捷径检查 PASS：
  - 无 T1 伪造（T1=no_data，不参与校准）
  - 无 Lum 替代（T2/T4 缺 Lum flat 的 Light 帧被跳过，未用其他滤镜 flat 替代）
  - 全部 Light/Master 路径真实存在（无伪造路径）
- 25/25 测试 PASS
- 证据：engineering_v1.2/evidence/P10-006/

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

## 下一步：P11-001

依据 `tasks/P11-001.md`（待查阅）：

- 冻结内部/图像/FITS/WCS坐标约定
- 依赖：P09-002（已满足）
- G10 Gate 已完成全部 6 个任务（P10-001 ~ P10-006）

## 已知 BLOCKED 项

- 123 Lum Light 帧（T2 25 + T4 98）缺 Lum Flat Master，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录。P10-006 中已正确跳过这些帧，未参与代表帧校准。
