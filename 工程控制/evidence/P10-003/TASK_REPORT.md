# 任务报告

- Task/ADR：P10-003 盘点全部主校准帧
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

依据 `tasks/P10-003.md` 和 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`，扫描 T1-T4 全部 Master Bias/Dark/Flat XISF 文件，记录 SHA-256 与 Header/文件名属性，输出 `CALIBRATION_MASTER_INVENTORY.csv`，作为 P10-004 (Light→Master 解析) 的输入。

## 输入与范围

- 输入：testdata/T2 calibration files/、testdata/T3 calibration files/、testdata/T4 calibration files/（T1 无校准数据）
- 依赖：P10-001（已满足，TESTDATA_EQUIPMENT_CATALOG.csv + FILTER_ALIAS_MAP.json 作为参照）
- 参考规范：`docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`
- 工具：`engineering_v1.2/evidence/P10-003/scripts/extract_calibration_master_inventory.py`
- 输出：`CALIBRATION_MASTER_INVENTORY.csv`（27 行）+ `CALIBRATION_MASTER_INVENTORY_SUMMARY.json`

## 执行/决策

### 阶段 1：发现校准目录

- 扫描 testdata/ 下子目录，匹配 `T{N} calibration files` 模式
- 发现 3 个校准目录：T2/T3/T4 calibration files
- T1 无校准目录（与 P10-002 一致，T1 status=no_data）

### 阶段 2：扫描 XISF 文件

对每个 XISF 文件提取：
- 文件元信息：file_path / file_name / file_size_bytes / sha256
- 设备与类型：device_id（从目录名提取）/ master_type（从文件名解析 Bias/Dark/Flat）
- 文件名属性：filter_from_filename / exposure_from_filename / bin_from_filename / image_size_from_filename
- Header 属性：filter_from_header / exposure_from_header / bin_from_header / image_size_from_header / temp_from_header / pixel_size_from_header / instrument_from_header
- 一致性检查：filter_match / exposure_match / bin_match / image_size_match（filename vs header）

### 阶段 3：解析 XISF Header

XISF 文件格式解析（兼容两种变体）：
- 标准 XISF：magic `XISF` + 4 字节版本 `0100` + 4 字节 header length + XML header
- PixInsight 变体：magic `XISF` + 4 字节 header length（直接）+ XML header（共 16 字节前缀）

XML namespace 处理：
- 去除 `xmlns="..."` / `xmlns:xsi="..."` / `xsi:schemaLocation="..."` 声明
- 使 `ET.fromstring` 用无 namespace 前缀的标签名

属性提取来源：
- `<Image>` 元素的 attribute（geometry/label/sampleFormat）
- `<Property>` 子元素（name/value 对）
- `<FITSKeyword>` 子元素（name/value 对，值去除首尾 `'`）

### 阶段 4：生成 CSV 与汇总

- CSV 27 行：T2(9) + T3(9) + T4(9) = 27
- 按设备：T2=9, T3=9, T4=9
- 按类型：Bias=3, Dark=8, Flat=16
- 汇总 JSON：包含 total_files/by_device/by_type/header_parse_errors/hard_gate_unknown_type

### 阶段 5：一致性检查（filename vs header）

27 行全部 4 项 match 为 YES：
- filter_match: 27/27 YES
- exposure_match: 27/27 YES
- bin_match: 27/27 YES
- image_size_match: 27/27 YES

### 阶段 6：运行验证脚本

`validate_calibration_master_inventory.py` 执行 20 项测试，全部 PASS。

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_calibration_master_inventory.py` | 60s | 0 |
| `python validate_calibration_master_inventory.py` | 60s | 0 |

## 结果与证据

### 交付物

1. **CALIBRATION_MASTER_INVENTORY.csv** — 27 行，22 列
   - 列：file_path/file_name/device_id/master_type/file_size_bytes/sha256/filter_from_filename/exposure_from_filename/bin_from_filename/image_size_from_filename/filter_from_header/exposure_from_header/bin_from_header/image_size_from_header/temp_from_header/pixel_size_from_header/instrument_from_header/filter_match/exposure_match/bin_match/image_size_match/header_parse_error/scanned_at
2. **CALIBRATION_MASTER_INVENTORY_SUMMARY.json** — 汇总统计

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

### 校准文件分布

| 设备 | Bias | Dark exposures (s) | Flat filters | 缺失 Flat |
|------|------|-------------------|--------------|-----------|
| T2 | 1 | [600, 1200, 1800] | Blue/Green/H-alpha/OIII/Red | Lum |
| T3 | 1 | [600, 1200] | Blue/Green/H-alpha/Lum/Oiii/Red | 无 |
| T4 | 1 | [180, 300, 600] | Blue/Green/H-alpha/Oiii/Red | Lum |

### 硬门限检查

- 全部 27 个文件有合法 SHA-256（64 hex 字符）：**PASS**
- 全部 27 个 SHA-256 唯一（无重复）：**PASS**
- master_type 集合 = {Bias, Dark, Flat}（无 Unknown）：**PASS**
- device_id 集合 = {T2, T3, T4} ⊆ {T1, T2, T3, T4}：**PASS**
- 禁止捷径（不得因 resolver 找不到就判定文件缺失）：**PASS**（全部文件均扫描并记录）

### 关键发现

1. **T1 无校准文件**：testdata 中无 T1 calibration files 目录，与 P10-002 T1=no_data 一致
2. **T2/T4 缺 Lum Flat**：T2/T4 仅有 5 个 Flat（Blue/Green/H-alpha/OIII/Red），无 Lum Flat，将在 P10-005 处理
3. **T3 Flat 完整**：6 个 Flat 覆盖 Blue/Green/H-alpha/Lum/Oiii/Red
4. **OIII/Oiii 别名不一致**：T2 用 OIII，T3/T4 用 Oiii，将在 P10-004 统一为规范名 OIII
5. **T2/T3 H-alpha Flat 曝光特殊**：T2=30s（其他 Flat 3s），T3=10s（其他 Flat 2s），可能是窄带 Flat
6. **T4 Flat 曝光非整数**：Green=10.53s，Oiii=10.28s，Red=6.80s（可能为非均匀光源的曝光自适应）
7. **Header 与文件名完全一致**：27/27 行 4 项 match 全为 YES，文件名模板与 Header 字段无冲突
8. **全部 Bin=1，全部 instrument=FLI**：与 P10-002 设备档案一致

## 风险/回滚/残留

- **T2/T4 缺 Lum Flat**：已在 P10-002 设备档案 missing_flats 记录，P10-005 校准阶段须特殊处理（借用其他设备 Flat 或跳过 Lum 校准）
- **OIII/Oiii 别名不一致**：本任务保留实际观测拼写，P10-004 冻结规范名后统一
- **T1 无校准文件**：硬门限允许（T1 status=no_data），不影响通过
- **H-alpha Flat 曝光特殊**：T2=30s, T3=10s（其他 Flat 为 2-3s），P10-004 匹配时须按滤镜规范名匹配，不按曝光

## 结论

P10-002 完成。27 个主校准帧全部盘点完成，CALIBRATION_MASTER_INVENTORY.csv（27 行 22 列）+ CALIBRATION_MASTER_INVENTORY_SUMMARY.json 已生成。20/20 测试 PASS。硬门限满足（无 Unknown 类型、SHA-256 全唯一、device_id ⊆ T1-T4）。禁止捷径检查通过（全部文件均扫描，无遗漏）。Header 与文件名一致性 27/27。T2/T4 缺 Lum Flat 已记录，OIII/Oiii 别名将在 P10-004 统一。
