# 证据索引

- Task/ADR：P10-004 冻结滤镜规范名和别名
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

从说明文档 / FITS+XISF Header / 文件名 三个来源建立规范名与别名的最终冻结映射，测试 Unicode/大小写变体。P10-001 阶段生成的初版仅含 7 个观察别名，本任务扩展为 52 别名完整映射表（含 Unicode 希腊字母 α、下标 ₃、罗马数字 Ⅲ、全角字符等变体）并冻结。

## 输入与范围

- 输入：7 个素材文档 + header_samples.json（49 FITS + 27 XISF）+ TESTDATA_DATASET_CATALOG.csv + 4 个 DEVICE_PROFILE.json + CALIBRATION_MASTER_INVENTORY.csv
- 依赖：P10-001 + P10-002（已满足）
- 参考规范：`docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`

## 执行/决策

1. 扫描所有数据源的滤镜名（11 个观察别名）
2. 构建 52 别名映射表（含 Unicode/全角/大小写变体）
3. 实现归一化函数（直接查表 + 大小写归一化）
4. 禁止捷径检查（6 规范名对应 6 个不同物理滤镜）
5. 构建 ALIAS_OBSERVATION_REPORT.json
6. 运行 23 项测试验证

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python freeze_filter_alias_map.py` | 60s | 0 |
| `python validate_filter_alias_map.py` | 60s | 0 |

## 结果与证据

### 证据目录结构

```
engineering_v1.2/evidence/P10-004/
├── TASK_REPORT.md                     # 任务报告
├── TEST_REPORT.md                     # 测试报告 (23/23 PASS)
├── EVIDENCE_INDEX.md                  # 本文件
├── REVIEW_REPORT.md                   # 独立复核报告
├── FILTER_ALIAS_MAP.json              # 主交付物: 52 别名冻结映射
├── ALIAS_OBSERVATION_REPORT.json      # 主交付物: 别名来源观察报告
├── scripts/
│   ├── freeze_filter_alias_map.py     # 冻结脚本
│   └── validate_filter_alias_map.py  # 验证脚本 (23 测试)
└── raw_logs/
    ├── freeze_filter_alias_map.log           # 冻结脚本运行日志
    ├── validate_filter_alias_map.log         # 验证脚本运行日志
    └── validate_filter_alias_map_result.json # 测试结果 JSON
```

### 关键统计

| 指标 | 值 |
|------|-----|
| 规范名数量 | 6 (LUM/RED/GREEN/BLUE/HA/OIII) |
| 别名总数 | 52 |
| 观察到的别名 | 11 |
| 无法归一化的别名 | 0 |
| 大小写测试 | 42/42 PASS |
| Unicode 希腊字母 α 测试 | 4/4 PASS |
| Unicode 下标 ₃ + 罗马数字 Ⅲ 测试 | 4/4 PASS |
| 全角字符测试 | 1/1 PASS |
| 往返校验 | 52/52 PASS |
| 与 P10-001 兼容性 | 22/22 旧别名可归一化 |
| 与 P10-002 交叉验证 | 7/7 profile filter_set 可归一化 |
| 与 P10-003 交叉验证 | 7/7 inventory filter 可归一化 |
| 测试通过率 | 23/23 (100%) |
| 硬门限 | PASS |

### 交付物 SHA-256

| 文件 | SHA-256 |
|------|---------|
| FILTER_ALIAS_MAP.json | 9B3A6FFFDA7F7752C5D9FAC78F2AC6172322F3D11348BCB906DDEB144C24BD0E |
| ALIAS_OBSERVATION_REPORT.json | 610B747B5573BD1A06315451E053F1CB88CE2CE4A80A886EDD2FCE0C4EFFA222 |
| freeze_filter_alias_map.py | DDD5CFBD126EF1ADFB2B5A8D232027D618382F04FC0B93C3FE10EB5144813682 |
| validate_filter_alias_map.py | 3B96B6D577C1B1C0DB9001F9B9832AD7FA61FB45F7832399658084CC5FDE2530 |

### 规范名 -> 首选别名 / 文档拼写 / 物理滤镜

| 规范名 | 首选别名（Header） | 文档拼写 | 物理滤镜 |
|--------|-------------------|---------|---------|
| LUM | Lum | Lum | Luminance 宽带滤镜 (透过整个可见光谱) |
| RED | Red | Red | Red 宽带 R 滤镜 |
| GREEN | Green | Green | Green 宽带 G 滤镜 |
| BLUE | Blue | Blue | Blue 宽带 B 滤镜 |
| HA | H-alpha | Halpha | H-alpha 窄带滤镜 (656.3nm 氢α发射线) |
| OIII | OIII | OIII | OIII 窄带滤镜 (500.7nm 双电离氧发射线) |

### 别名分类（52 项）

| 类别 | 数量 | 举例 |
|------|------|------|
| 大小写变体 | 32 | L/l/LUM/Lum/lum, R/r/RED/Red/red, OIII/Oiii/oiii/oIII |
| Unicode 希腊字母 α | 4 | Hα (U+03B1), hα, HΑ (U+0391), α |
| Unicode 下标 ₃ | 2 | O₃ (U+2083), o₃ |
| Unicode 罗马数字 Ⅲ | 2 | OⅢ (U+2162), oⅢ |
| 全角字符 | 1 | ＯＩＩＩ (U+FF2F U+FF29 U+FF29 U+FF29) |
| 连字符/分隔 | 5 | H-alpha/H-Alpha/h-alpha, O-III/o-iii |
| 数字变体 | 2 | O3, o3 |
| 规范名本身 | 6 | LUM, RED, GREEN, BLUE, HA, OIII |

### 观察到的别名来源（11 个）

| 别名 | 来源 |
|------|------|
| L | 文档（5 个 .txt）, device profile filter_set_doc |
| R | 文档（5 个 .txt）, device profile filter_set_doc |
| G | 文档（5 个 .txt）, device profile filter_set_doc |
| B | 文档（5 个 .txt）, device profile filter_set_doc |
| Lum | 文档（3 个 .txt）, FITS Header, XISF Header, dataset_catalog, calibration_inventory, device profile filter_set |
| Red | 文档（6 个 .txt）, FITS Header, XISF Header, dataset_catalog, calibration_inventory, device profile filter_set |
| Green | 文档（6 个 .txt）, FITS Header, XISF Header, dataset_catalog, calibration_inventory, device profile filter_set |
| Blue | 文档（6 个 .txt）, FITS Header, XISF Header, dataset_catalog, calibration_inventory, device profile filter_set |
| H-alpha | FITS Header, XISF Header, dataset_catalog, calibration_inventory, device profile filter_set |
| OIII | 文档（4 个 .txt）, FITS Header (T2), XISF Header (T2), dataset_catalog, calibration_inventory (T2), device profile filter_set (T2) |
| Oiii | FITS Header (T3/T4), XISF Header (T3/T4), dataset_catalog (T3/T4), calibration_inventory (T3/T4), device profile filter_set (T3/T4) |

### 失败基线与修复

| 问题 | 现象 | 修复 | 验证 |
|------|------|------|------|
| OIII/Oiii 别名冲突 | T2 用 OIII, T3/T4 用 Oiii | 全部映射到规范名 OIII | T20 PASS |
| H-alpha 拼写不一致 | Header 用 H-alpha, 文档用 Halpha/Ha | 全部映射到规范名 HA | T09 PASS |
| P10-001 alias map 不含 Unicode | 仅 22 个 ASCII 别名 | 扩展为 52 个别名（含 Unicode） | T10-T12 PASS |

### 与 P10-001/P10-002/P10-003 交叉验证

| 数据源 | 观察别名数 | 可归一化数 | 一致性 |
|--------|-----------|-----------|--------|
| P10-001 FILTER_ALIAS_MAP.json | 22 | 22 | ✅ |
| P10-002 device profile filter_set | 7 | 7 | ✅ |
| P10-003 CALIBRATION_MASTER_INVENTORY | 7 | 7 | ✅ |

## 风险/回滚/残留

- 41 个别名是预防性扩展（Unicode/全角/大小写变体），实际观察到 11 个
- 文档滤镜行格式不统一（7 种变体），regex 提取已覆盖
- OIII/Oiii 别名冲突已解决（统一映射到 OIII）
- H-alpha 拼写不一致已解决（统一映射到 HA）

## 结论

P10-004 完成。2 个交付物已生成（FILTER_ALIAS_MAP.json + ALIAS_OBSERVATION_REPORT.json），硬门限 PASS。23/23 测试通过。6 个规范名（LUM/RED/GREEN/BLUE/HA/OIII）冻结，52 个别名（含 Unicode 希腊字母 α、下标 ₃、罗马数字 Ⅲ、全角字符）全部可归一化。禁止捷径检查通过（6 个规范名对应 6 个不同物理滤镜，无合并）。与 P10-001/P10-002/P10-003 观察到的 11 个别名交叉验证全部可归一化。OIII/Oiii 别名冲突已解决。
