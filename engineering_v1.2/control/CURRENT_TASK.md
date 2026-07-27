# 当前任务

`P10-004`：冻结滤镜规范名和别名。

## P10-003 已完成（2026-07-27）

- 2 个交付物：
  - CALIBRATION_MASTER_INVENTORY.csv（27 行 22 列：T2:9 + T3:9 + T4:9 = 3 Bias + 8 Dark + 16 Flat）
  - CALIBRATION_MASTER_INVENTORY_SUMMARY.json（汇总统计）
- 全部 27 个 XISF 文件 SHA-256 全唯一
- 全部 27 行 4 项 filename/header match 全为 YES（filter/exposure/bin/image_size）
- Header 解析错误 0，Unknown 类型 0
- 与 P10-002 设备档案交叉验证一致（Dark 曝光、Flat 滤镜、缺失 Lum Flat）
- 硬门限 PASS，20/20 测试 PASS
- 证据：engineering_v1.2/evidence/P10-003/

## 历史任务（已完成）

- P09-001：v1.1 基线冻结 + v1.2 开发包安装（4 件套）
- P09-002：INTERNAL_DETECTION_SHARED_EXPORT 命名统一（6/6 PASS）
- P09-003：canonical_dataset_v1.2 冻结（44 文件 SHA-256，7 测光失败帧，4 HCSD 基线）
- P10-001：TestData 目录盘点（3 设备 / 49 light 组 / 49+27 Header 采样）
- P10-002：T1-T4 设备档案建立（4 profile + summary，710 lights，76/76 PASS）
- P10-003：主校准帧盘点（27 文件 CSV + summary，20/20 PASS）

## 下一步：P10-004

依据 `tasks/P10-004.md`：

- 冻结滤镜规范名和别名
- 依赖：P10-001, P10-002（均已满足）
- 参考：`docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`
- 预期交付：FILTER_ALIAS_MAP.json + alias tests
- 禁止捷径：不得将不同物理滤镜错误合并

P10-003 已完成，P10-004 依赖（P10-001 + P10-002）均已满足，可立即执行。

P10-005 依赖 P10-002 + P10-003 + P10-004，需 P10-004 完成后才能开始。
