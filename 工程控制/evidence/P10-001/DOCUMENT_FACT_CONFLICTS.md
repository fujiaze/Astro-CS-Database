# 文档与 Header 事实冲突报告

- 任务: P10-001
- 生成时间: 2026-07-27T19:13:15.790419
- 测试数据目录: testdata/
- 发现子目录数: 10
- 设备数 (T1-T4): 3
- 未知设备目录: 0
- Light 分组数 (target/device/panel/filter): 49
- Header 采样数: FITS=49, XISF=27
- 冲突数: 1

## 冲突明细

### 冲突 1: calibration_filter_alias_inconsistency

- 设备: N/A
- canonical: OIII
- aliases: ['OIII', 'Oiii']
- note: 校准文件中同一规范滤镜使用了不同的别名拼写, 必须在 P10-004 统一

## 硬门限检查

- 只允许 T1-T4 四套规范设备 ID: PASS
  - 实际设备: ['T2', 'T3', 'T4']
- 所有 Light 必须能归属 T1-T4: PASS
