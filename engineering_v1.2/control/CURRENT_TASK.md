# 当前任务

`P10-005`：实现并验证 Light 到 Master 唯一解析。

## P10-004 已完成（2026-07-27）

- 2 个交付物：
  - FILTER_ALIAS_MAP.json（52 别名映射到 6 规范名 LUM/RED/GREEN/BLUE/HA/OIII）
  - ALIAS_OBSERVATION_REPORT.json（11 个观察别名的来源报告）
- 覆盖 Unicode 希腊字母 α（U+03B1）4 项 + 下标 ₃（U+2083）2 项 + 罗马数字 Ⅲ（U+2162）2 项 + 全角字符 1 项
- 大小写归一化 42 项全 PASS，往返校验 52/52 全 PASS
- OIII/Oiii 别名冲突已解决（统一映射到 OIII）
- H-alpha 拼写不一致已解决（统一映射到 HA）
- 与 P10-001 兼容性 22/22, P10-002/P10-003 交叉验证 7+7 全可归一化
- 硬门限 PASS，23/23 测试 PASS
- 证据：engineering_v1.2/evidence/P10-004/

## 历史任务（已完成）

- P09-001：v1.1 基线冻结 + v1.2 开发包安装（4 件套）
- P09-002：INTERNAL_DETECTION_SHARED_EXPORT 命名统一（6/6 PASS）
- P09-003：canonical_dataset_v1.2 冻结（44 文件 SHA-256，7 测光失败帧，4 HCSD 基线）
- P10-001：TestData 目录盘点（3 设备 / 49 light 组 / 49+27 Header 采样）
- P10-002：T1-T4 设备档案建立（4 profile + summary，710 lights，76/76 PASS）
- P10-003：主校准帧盘点（27 文件 CSV + summary，20/20 PASS）
- P10-004：滤镜规范名与别名冻结（52 别名映射，23/23 PASS）

## 下一步：P10-005

依据 `tasks/P10-005.md`：

- 实现并验证 Light 到 Master 唯一解析
- 依赖：P10-002 + P10-003 + P10-004（均已满足）
- 参考：`docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`
- 预期交付：LIGHT_TO_MASTER_RESOLUTION.csv + resolver tests
- 禁止捷径：不得 first-match 或静默选择歧义

P10-004 已完成，P10-005 依赖（P10-002 + P10-003 + P10-004）均已满足，可立即执行。

P10-005 是 G10 Gate 的关键任务，输出每张 Light 的选择理由和歧义，全部 710 Light 帧必须解析到唯一的 Bias/Dark/Flat Master。
