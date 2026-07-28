# 证据索引

- Task/ADR：P10-003 盘点全部主校准帧
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

扫描 T1-T4 全部 Master Bias/Dark/Flat XISF 文件，记录 SHA-256 与 Header/文件名属性，输出 CALIBRATION_MASTER_INVENTORY.csv，作为 P10-004 (Light→Master 解析) 的输入。

## 输入与范围

- 输入：testdata/T2 calibration files/ + testdata/T3 calibration files/ + testdata/T4 calibration files/（27 个 XISF）
- 依赖：P10-001（已满足，TESTDATA_EQUIPMENT_CATALOG.csv + FILTER_ALIAS_MAP.json 作为参照）
- 参考规范：`docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`

## 执行/决策

1. 发现 3 个校准目录（T2/T3/T4 calibration files）
2. 扫描 27 个 XISF 文件（T2:9 + T3:9 + T4:9 = 27）
3. 解析 XISF Header（兼容标准 + PixInsight 16 字节变体）
4. 提取文件名与 Header 属性，进行 4 项一致性检查
5. 计算 SHA-256，生成 CSV（27 行 22 列）+ SUMMARY.json
6. 运行 20 项测试验证

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_calibration_master_inventory.py` | 60s | 0 |
| `python validate_calibration_master_inventory.py` | 60s | 0 |

## 结果与证据

### 证据目录结构

```
engineering_v1.2/evidence/P10-003/
├── TASK_REPORT.md                            # 任务报告
├── TEST_REPORT.md                            # 测试报告 (20/20 PASS)
├── EVIDENCE_INDEX.md                         # 本文件
├── REVIEW_REPORT.md                          # 独立复核报告
├── CALIBRATION_MASTER_INVENTORY.csv          # 主交付物: 27 行校准帧清单
├── CALIBRATION_MASTER_INVENTORY_SUMMARY.json # 主交付物: 汇总统计
├── scripts/
│   ├── extract_calibration_master_inventory.py  # 生成脚本
│   └── validate_calibration_master_inventory.py # 验证脚本 (20 测试)
└── raw_logs/
    ├── extract_calibration_master_inventory.log        # 生成脚本运行日志
    ├── validate_calibration_master_inventory.log       # 验证脚本运行日志
    └── validate_calibration_master_inventory_result.json # 测试结果 JSON
```

### 关键统计

| 指标 | 值 |
|------|-----|
| 文件总数 | 27 |
| T2 文件数 | 9 (1 Bias + 3 Dark + 5 Flat) |
| T3 文件数 | 9 (1 Bias + 2 Dark + 6 Flat) |
| T4 文件数 | 9 (1 Bias + 3 Dark + 5 Flat) |
| Bias 总数 | 3 |
| Dark 总数 | 8 |
| Flat 总数 | 16 |
| Header 解析错误 | 0 |
| Unknown 类型 | 0 |
| SHA-256 重复 | 0 |
| 4 项 match 全 YES | 27/27 |
| 测试通过率 | 20/20 (100%) |
| 硬门限 | PASS |

### 交付物 SHA-256

| 文件 | SHA-256 |
|------|---------|
| CALIBRATION_MASTER_INVENTORY.csv | 58F7A9A95BE717C736C096E1D418A11AFD376E00257BA068C097D27AD54FFBFC |
| CALIBRATION_MASTER_INVENTORY_SUMMARY.json | EB1D7B8877FFB0254AB9DF474BBE84D3089B93CE3140A9AE2BF7164795F3319A |
| extract_calibration_master_inventory.py | 40DFC5B8DBCC8B85AAD0531374103D702D13067F0DBA748AB133F15739A24E0F |
| validate_calibration_master_inventory.py | E051B86B8911C624CE0C53394C9D019CDA70583C6EDBF6AD970BB5EE70B78295 |

### 校准文件分布

| 设备 | Bias | Dark (曝光 s) | Flat (滤镜) | 缺失 Flat |
|------|------|---------------|-------------|-----------|
| T2 | 1 | [600, 1200, 1800] | Blue, Green, H-alpha, OIII, Red | Lum |
| T3 | 1 | [600, 1200] | Blue, Green, H-alpha, Lum, Oiii, Red | 无 |
| T4 | 1 | [180, 300, 600] | Blue, Green, H-alpha, Oiii, Red | Lum |

### CSV 列定义（22 列）

| 列名 | 说明 |
|------|------|
| file_path | 相对仓库根的路径 |
| file_name | 文件名 |
| device_id | T2/T3/T4（从目录名提取） |
| master_type | Bias/Dark/Flat（从文件名解析） |
| file_size_bytes | 文件字节数 |
| sha256 | SHA-256 哈希（64 hex） |
| filter_from_filename | 文件名解析的滤镜名 |
| exposure_from_filename | 文件名解析的曝光（s） |
| bin_from_filename | 文件名解析的 Bin |
| image_size_from_filename | 文件名解析的图像尺寸 |
| filter_from_header | Header 读取的滤镜名 |
| exposure_from_header | Header 读取的曝光（s） |
| bin_from_header | Header 读取的 Bin |
| image_size_from_header | Header 读取的图像尺寸 |
| temp_from_header | Header 读取的温度（°C） |
| pixel_size_from_header | Header 读取的像元尺寸（um） |
| instrument_from_header | Header 读取的相机厂商 |
| filter_match | filename vs header 滤镜一致性 YES/NO |
| exposure_match | filename vs header 曝光一致性 YES/NO |
| bin_match | filename vs header Bin 一致性 YES/NO |
| image_size_match | filename vs header 尺寸一致性 YES/NO |
| header_parse_error | Header 解析错误信息（空=无错误） |
| scanned_at | 扫描时间戳 ISO 8601 UTC |

### 与 P10-002 设备档案交叉验证

| 字段 | P10-002 (DEVICE_PROFILE) | P10-003 (INVENTORY) | 一致性 |
|------|-------------------------|----------------------|--------|
| T2 dark_exposures_s | [600, 1200, 1800] | [600, 1200, 1800] | ✅ |
| T3 dark_exposures_s | [600, 1200] | [600, 1200] | ✅ |
| T4 dark_exposures_s | [180, 300, 600] | [180, 300, 600] | ✅ |
| T2 missing_flats | ["Lum"] | T2 Flats = 5 项（无 Lum） | ✅ |
| T3 missing_flats | [] | T3 Flats = 6 项（含 Lum） | ✅ |
| T4 missing_flats | ["Lum"] | T4 Flats = 5 项（无 Lum） | ✅ |
| T2 calib 文件数 | 9 | 9 | ✅ |
| T3 calib 文件数 | 9 | 9 | ✅ |
| T4 calib 文件数 | 9 | 9 | ✅ |

### 失败基线与修复

| 问题 | 现象 | 修复 | 验证 |
|------|------|------|------|
| XISF namespace 解析失败 | ET.fromstring 因 xmlns 前缀返回带 namespace 的标签 | 用 regex 去除 xmlns/xmlns:xsi/xsi: 声明 | T05/T20 PASS |
| PixInsight 16 字节 XISF Header 变体 | 标准 8 字节偏移读取失败 | 兼容 `bytes_4_7 == b"0100"` 和直接 4 字节 length 两种格式 | T02 PASS |

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（P10-005 处理）
- OIII/Oiii 别名不一致（P10-004 统一）
- T1 无校准文件（硬门限允许）
- H-alpha Flat 曝光特殊（T2=30s, T3=10s，窄带 Flat），P10-004 须按滤镜规范名匹配

## 结论

P10-003 完成。2 个交付物已生成（CALIBRATION_MASTER_INVENTORY.csv 27 行 + SUMMARY.json），硬门限 PASS。20/20 测试通过。27 个主校准帧全部盘点，SHA-256 全唯一，4 项 filename/header match 全为 YES。与 P10-002 设备档案交叉验证完全一致。禁止捷径检查通过（无 Unknown 类型、无 parse error、全部文件均扫描）。
