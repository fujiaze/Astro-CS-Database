# 测试报告

- Task/ADR：P10-003 盘点全部主校准帧
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

验证 P10-003 的 CALIBRATION_MASTER_INVENTORY.csv 完整性与硬门限，确保：
1. 入口条件与依赖状态满足（P10-001 已完成）
2. CSV 存在且可读，行数 = 27（T2:9 + T3:9 + T4:9）
3. 全部行有合法 SHA-256（64 hex 字符）且唯一
4. master_type 集合 = {Bias, Dark, Flat}（无 Unknown）
5. device_id 集合 ⊆ {T1, T2, T3, T4}
6. 各设备 Bias/Dark/Flat 分布符合 P10-002 设备档案
7. Dark 曝光覆盖与 P10-002 一致
8. Flat 滤镜集合与 P10-002 一致（含缺失 Lum 记录）
9. 全部 bin = 1，全部 instrument = FLI
10. filename vs header 4 项 match 全为 YES
11. 禁止捷径检查（无 Unknown 类型、无 Header 解析错误、无遗漏）

## 输入与范围

- 测试数据：P10-003 生成的 CALIBRATION_MASTER_INVENTORY.csv（27 行）
- 测试脚本：`scripts/validate_calibration_master_inventory.py`（20 项测试）
- 测试方法：自动校验 + CSV schema 检查 + 与 P10-001/P10-002 交叉验证
- 参照：P10-001 TESTDATA_EQUIPMENT_CATALOG.csv + FILTER_ALIAS_MAP.json，P10-002 DEVICE_PROFILE_SUMMARY.json

## 执行/决策

### 测试矩阵

| 测试项 | 类型 | 必测项 | 状态 |
|--------|------|--------|------|
| T01 入口条件 (P10-001 交付物) | contract | 入口条件 | PASS |
| T02 CSV 存在且可读 | contract | 对应测试 | PASS |
| T03 行数 = 27 (T2:9 + T3:9 + T4:9) | contract | 真实数据测试 | PASS |
| T04 device_id 集合 ⊆ T1-T4 | contract | 全部 | PASS |
| T05 master_type 集合 = {Bias,Dark,Flat} | contract | 全部 | PASS |
| T06 全部行 SHA-256 合法 (64 hex) | unit | 对应测试 | PASS |
| T07 全部 file_size_bytes > 0 | unit | 对应测试 | PASS |
| T08 全部行有 scanned_at 时间戳 | unit | 对应测试 | PASS |
| T09 T2/T3/T4 各 1 个 Bias | unit | 真实数据测试 | PASS |
| T10 T2 Dark exposures = [600,1200,1800] | unit | 真实数据测试 | PASS |
| T11 T3 Dark exposures = [600,1200] | unit | 真实数据测试 | PASS |
| T12 T4 Dark exposures = [180,300,600] | unit | 真实数据测试 | PASS |
| T13 T2 Flats = 5 项 (缺 Lum) | unit | 真实数据测试 | PASS |
| T14 T3 Flats = 6 项 (完整) | unit | 真实数据测试 | PASS |
| T15 T4 Flats = 5 项 (缺 Lum) | unit | 真实数据测试 | PASS |
| T16 全部 bin = 1 | unit | 真实数据测试 | PASS |
| T17 全部 instrument = FLI | unit | 真实数据测试 | PASS |
| T18 全部 4 项 match 全为 YES | unit | 对应测试 | PASS |
| T19 全部 SHA-256 唯一 (无重复) | unit | 对应测试 | PASS |
| T20 禁止捷径 (无 Unknown/无 parse error) | contract | 全部 | PASS |

### 测试详情

**T01 入口条件**
- 依赖 P10-001 已完成（DONE）
- P10-001 交付物齐全：TESTDATA_EQUIPMENT_CATALOG.csv + FILTER_ALIAS_MAP.json
- PASS

**T02 CSV 存在且可读**
- CALIBRATION_MASTER_INVENTORY.csv 存在
- csv.DictReader 成功读取 27 行
- PASS

**T03 行数 = 27**
- 总行数 27（含表头共 28 行）
- T2:9 + T3:9 + T4:9 = 27
- PASS

**T04 device_id 集合**
- 实际 device_id 集合 = {T2, T3, T4}
- ⊆ {T1, T2, T3, T4}（T1 无校准数据，符合预期）
- PASS

**T05 master_type 集合**
- 实际 master_type 集合 = {Bias, Dark, Flat}
- 无 Unknown 类型
- PASS

**T06 SHA-256 合法性**
- 27 行全部有 SHA-256
- 全部 64 位十六进制字符
- PASS

**T07 file_size_bytes > 0**
- 27 行全部 file_size_bytes > 0
- 最小 64820480（约 64 MB），最大 67133440（约 64 MB）
- PASS

**T08 scanned_at 时间戳**
- 27 行全部有 scanned_at
- 格式 ISO 8601 UTC：2026-07-27T11:38:0x.xxxxxxZ
- PASS

**T09 Bias 分布**
- T2: 1 Bias（masterBias_BIN-1_4096x4096.xisf）
- T3: 1 Bias（masterBias_BIN-1_4096x4096.xisf）
- T4: 1 Bias（masterBias_BIN-1_4500x3600.xisf）
- PASS

**T10 T2 Dark exposures**
- T2 Dark exposures = [600.0, 1200.0, 1800.0]
- 与 P10-002 T2_DEVICE_PROFILE.json 的 dark_exposures_s 一致
- PASS

**T11 T3 Dark exposures**
- T3 Dark exposures = [600.0, 1200.0]
- 与 P10-002 T3_DEVICE_PROFILE.json 的 dark_exposures_s 一致
- PASS

**T12 T4 Dark exposures**
- T4 Dark exposures = [180.0, 300.0, 600.0]
- 与 P10-002 T4_DEVICE_PROFILE.json 的 dark_exposures_s 一致
- PASS

**T13 T2 Flats**
- T2 Flats = [Blue, Green, H-alpha, OIII, Red]（5 项）
- 缺 Lum Flat（与 P10-002 T2 missing_flats=["Lum"] 一致）
- PASS

**T14 T3 Flats**
- T3 Flats = [Blue, Green, H-alpha, Lum, Oiii, Red]（6 项）
- 完整（与 P10-002 T3 missing_flats=[] 一致）
- PASS

**T15 T4 Flats**
- T4 Flats = [Blue, Green, H-alpha, Oiii, Red]（5 项）
- 缺 Lum Flat（与 P10-002 T4 missing_flats=["Lum"] 一致）
- PASS

**T16 bin = 1**
- 27 行全部 bin = 1（集合 = {'1'}）
- PASS

**T17 instrument = FLI**
- 27 行全部 instrument = FLI
- PASS

**T18 4 项 match 全为 YES**
- 27 行全部 filter_match=YES
- 27 行全部 exposure_match=YES
- 27 行全部 bin_match=YES
- 27 行全部 image_size_match=YES
- 0 个 NO
- PASS

**T19 SHA-256 唯一性**
- 27 个 SHA-256 全部唯一
- 无重复（即无重复文件）
- PASS

**T20 禁止捷径**
- 无 Unknown 类型
- 无 Header 解析错误（header_parse_error 列全空）
- 全部 27 个文件均被扫描并记录
- PASS

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_calibration_master_inventory.py` | 60s | 0 |
| `python validate_calibration_master_inventory.py` | 60s | 0 |

## 结果与证据

- **20/20 测试 PASS**（详见 raw_logs/validate_calibration_master_inventory.log）
- 2 个交付物完整（CSV + SUMMARY.json）
- 硬门限 PASS（无 Unknown 类型、SHA-256 全唯一、device_id ⊆ T1-T4）
- 与 P10-002 设备档案交叉验证一致（Dark 曝光、Flat 滤镜、缺失 Lum Flat）
- 禁止捷径检查通过（全部 27 个文件均扫描，无遗漏、无 Unknown、无 parse error）

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（P10-005 处理）
- OIII/Oiii 别名不一致（P10-004 统一）
- T1 无校准文件（硬门限允许）

## 结论

P10-003 测试全部通过。20/20 测试 PASS。27 个主校准帧全部盘点完整，CALIBRATION_MASTER_INVENTORY.csv（27 行 22 列）+ SUMMARY.json 已生成。硬门限满足。与 P10-002 设备档案交叉验证一致（Dark 曝光、Flat 滤镜、缺失 Lum Flat 完全对齐）。禁止捷径检查通过（全部文件均扫描，无 Unknown、无 parse error、无遗漏）。
