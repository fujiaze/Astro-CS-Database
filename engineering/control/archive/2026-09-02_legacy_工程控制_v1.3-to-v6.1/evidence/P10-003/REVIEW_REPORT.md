# 复核报告

- Task/ADR：P10-003 盘点全部主校准帧
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

独立复核 P10-003 是否满足任务定义 `tasks/P10-003.md` 和规范 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md` 的全部要求。

## 输入与范围

- 任务定义：tasks/P10-003.md
- 规范：docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md
- 交付物：CALIBRATION_MASTER_INVENTORY.csv（27 行）+ CALIBRATION_MASTER_INVENTORY_SUMMARY.json
- 脚本：scripts/extract_calibration_master_inventory.py + scripts/validate_calibration_master_inventory.py
- 原始日志：raw_logs/

## 执行/决策

### 复核矩阵

| 复核项 | 验证内容 | 结果 |
|--------|----------|------|
| R-01 任务要求覆盖 | 任务定义的全部要求是否满足 | PASS |
| R-02 交付物完整性 | 2 个交付物（CSV + SUMMARY.json）是否全部生成 | PASS |
| R-03 文件数与分布 | 27 文件（T2:9 + T3:9 + T4:9 = 27） | PASS |
| R-04 类型分布 | Bias:3 + Dark:8 + Flat:16 = 27，无 Unknown | PASS |
| R-05 SHA-256 完整性与唯一性 | 27 行全有 SHA-256，全部唯一 | PASS |
| R-06 filename vs header 一致性 | 27 行 4 项 match 全为 YES | PASS |
| R-07 与 P10-002 设备档案交叉验证 | Dark 曝光/Flat 滤镜/缺失 Lum Flat 一致 | PASS |
| R-08 失败基线修复验证 | XISF namespace/PixInsight 16 字节变体已修复 | PASS |
| R-09 测试覆盖与通过率 | 20/20 测试 PASS | PASS |
| R-10 禁止捷径检查 | 无 Unknown 类型、无 parse error、全部文件均扫描 | PASS |

### R-01 任务要求覆盖

任务定义要求：
1. ✅ 扫描所有 Master Bias/Dark/Flat 及 Header/文件名属性，记录 hash
   - 27 个 XISF 文件全部扫描
   - 文件名属性（filter/exposure/bin/image_size）+ Header 属性（filter/exposure/bin/image_size/temp/pixel_size/instrument）提取
   - SHA-256 hash 全部记录
2. ✅ 输出 CALIBRATION_MASTER_INVENTORY.csv
   - 27 行 22 列，路径 engineering_v1.2/evidence/P10-003/CALIBRATION_MASTER_INVENTORY.csv

规范要求（docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md）：
1. ✅ 用户确认全部 Master 已配齐
   - 27 个文件全部扫描，无遗漏
2. ✅ 输出 CALIBRATION_MASTER_INVENTORY.csv（P10-003 交付物），LIGHT_TO_MASTER_RESOLUTION.csv（P10-004 交付物），UNRESOLVED_CALIBRATION_REPORT.md（P10-004 交付物）
   - P10-003 范围仅限 CALIBRATION_MASTER_INVENTORY.csv
3. ✅ 禁止捷径：不得因当前 resolver 找不到就判定文件缺失
   - 本任务不涉及 resolver，仅盘点
   - 全部 27 个文件均扫描并记录，无遗漏

### R-02 交付物完整性

| 交付物 | 状态 | 行数/大小 |
|--------|------|-----------|
| CALIBRATION_MASTER_INVENTORY.csv | PASS | 27 行 + 表头，22 列 |
| CALIBRATION_MASTER_INVENTORY_SUMMARY.json | PASS | JSON 包含 total_files/by_device/by_type/header_parse_errors/hard_gate_unknown_type |

### R-03 文件数与分布

- 总文件数：27
- T2:9 + T3:9 + T4:9 = 27 ✅
- 与 P10-002 DEVICE_PROFILE_SUMMARY 的 total_calib_files（T2:9, T3:9, T4:9）一致 ✅
- T1 无校准文件（与 P10-002 T1=no_data 一致）✅

### R-04 类型分布

- Bias:3（T2/T3/T4 各 1）
- Dark:8（T2:3 + T3:2 + T4:3）
- Flat:16（T2:5 + T3:6 + T4:5）
- 总和 3+8+16=27 ✅
- 无 Unknown 类型 ✅
- hard_gate_unknown_type = "PASS" ✅

### R-05 SHA-256 完整性与唯一性

- 27 行全部有 SHA-256（64 位十六进制字符）
- 27 个 SHA-256 全部唯一（无重复）
- SHA-256 列举（部分）：
  - T2 Bias: 2D7E694C536C9A12D3FF3D4ACF1565D88AD9495EDBC4C5F48EC242A230193303
  - T2 Dark 1200: 73D42C70DD85301537EE8F3DFDB7D97C8A0266DCCF9AAD78E652A3BC7B06AD48
  - T3 Bias: 22003F86CD3E11D51EF61149950D51AFEF2A570ED4BDD0FA9C6EEA434E834CF7
  - T4 Bias: EA14B6F5E471F673751C7E95BFCE8B92F09A135E0DB74AA4D277F3B4683E0D22
- 全部唯一 ✅

### R-06 filename vs header 一致性

27 行全部 4 项 match = YES：
- filter_match: 27/27 YES（0 NO）
- exposure_match: 27/27 YES（0 NO）
- bin_match: 27/27 YES（0 NO）
- image_size_match: 27/27 YES（0 NO）
- header_parse_error 列全空（无解析错误）
- PASS

### R-07 与 P10-002 设备档案交叉验证

| 字段 | P10-002 | P10-003 | 一致性 |
|------|---------|---------|--------|
| T2 calib 文件数 | 9 | 9 | ✅ |
| T3 calib 文件数 | 9 | 9 | ✅ |
| T4 calib 文件数 | 9 | 9 | ✅ |
| T2 dark_exposures_s | [600, 1200, 1800] | [600, 1200, 1800] | ✅ |
| T3 dark_exposures_s | [600, 1200] | [600, 1200] | ✅ |
| T4 dark_exposures_s | [180, 300, 600] | [180, 300, 600] | ✅ |
| T2 missing_flats | ["Lum"] | T2 Flats=5（无 Lum） | ✅ |
| T3 missing_flats | [] | T3 Flats=6（含 Lum） | ✅ |
| T4 missing_flats | ["Lum"] | T4 Flats=5（无 Lum） | ✅ |
| T2/T3/T4 image_size | 4096x4096 / 4096x4096 / 4500x3600 | 同上 | ✅ |
| 全部 bin | 1 | 1 | ✅ |
| 全部 instrument | FLI | FLI | ✅ |

所有字段与 P10-002 一致。PASS。

### R-08 失败基线修复验证

修复前问题：
1. **XISF namespace 解析失败**：`ET.fromstring` 因 XML 含 `xmlns="..."` 声明，返回的标签名带 namespace 前缀（如 `{http://...}Image`），导致 `root.iter("Image")` 无法匹配
2. **PixInsight 16 字节 Header 变体**：标准 XISF 是 4 字节 magic + 4 字节版本 `0100` + 4 字节 length + XML header（共 12 字节前缀），但 PixInsight 某些版本为 4 字节 magic + 4 字节 length（直接）+ XML header（共 8 字节前缀，加上 magic 后共 16 字节）

修复方案：
1. 用 regex 去除 `xmlns="..."` / `xmlns:xsi="..."` / `xsi:schemaLocation="..."` 声明
2. 检测 `bytes_4_7 == b"0100"` 区分两种格式，分别读取 header length

验证：
- T02 测试 CSV 可读（27 行）→ Header 解析成功
- T05 测试 master_type 集合 = {Bias, Dark, Flat}（无 Unknown）→ 文件名解析正确
- T20 测试无 parse error → Header 解析无错误
- 27 行全部 4 项 match = YES → filename 与 Header 数据一致

PASS。

### R-09 测试覆盖与通过率

- 测试脚本：scripts/validate_calibration_master_inventory.py
- 测试点数：20（覆盖 contract/unit/真实数据/失败基线/禁止捷径）
- 通过率：20/20 (100%)
- 失败数：0
- 跳过数：0
- PASS

### R-10 禁止捷径检查

- 无 Unknown 类型 ✅
- 无 Header 解析错误（header_parse_error 列全空）✅
- 全部 27 个文件均扫描并记录（无遗漏）✅
- 无未声明 fallback ✅
- 无数据范围缩减（T2/T3/T4 各 9 个文件全部计入）✅
- T1 无校准文件属真实情况（与 P10-002 一致），非捷径 ✅
- PASS

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_calibration_master_inventory.py` | 60s | 0 |
| `python validate_calibration_master_inventory.py` | 60s | 0 |

## 结果与证据

- 10/10 复核项 PASS
- 2 个交付物完整（CSV 27 行 + SUMMARY.json）
- 20/20 测试通过
- 硬门限 PASS（无 Unknown 类型、SHA-256 全唯一、device_id ⊆ T1-T4）
- 与 P10-002 设备档案交叉验证完全一致
- 失败基线（XISF namespace + 16 字节 Header 变体）已修复并验证
- 禁止捷径检查通过

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（已在 P10-002 missing_flats 记录，P10-005 处理）
- OIII/Oiii 别名不一致（P10-004 统一）
- T1 无校准文件（硬门限允许，T1=no_data）
- H-alpha Flat 曝光特殊（T2=30s, T3=10s，P10-004 须按滤镜规范名匹配）
- T4 Flat 曝光非整数（Green=10.53s, Oiii=10.28s, Red=6.80s），P10-004 须容忍小数曝光

## 结论

P10-003 独立复核通过。10 项复核全部 PASS。2 个交付物完整（CALIBRATION_MASTER_INVENTORY.csv 27 行 + SUMMARY.json），20/20 测试通过，硬门限满足。27 个主校准帧全部盘点，SHA-256 全唯一，4 项 filename/header match 全为 YES。与 P10-002 设备档案交叉验证完全一致（Dark 曝光、Flat 滤镜、缺失 Lum Flat、image_size、bin、instrument 全部对齐）。失败基线（XISF namespace + 16 字节 Header 变体）已修复并验证。禁止捷径检查通过（无 Unknown 类型、无 parse error、全部文件均扫描）。

VERDICT: PASS
