# 测试报告

- Task/ADR：P10-004 冻结滤镜规范名和别名
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

验证 P10-004 的 FILTER_ALIAS_MAP.json 完整性与 Unicode/大小写归一化，确保：
1. 入口条件与依赖状态满足（P10-001 + P10-002 + P10-003 已完成）
2. FILTER_ALIAS_MAP.json 存在且 JSON 可解析
3. 6 个规范名正确（LUM/RED/GREEN/BLUE/HA/OIII）
4. 禁止捷径：每个规范名对应不同物理滤镜（不合并）
5. 全部观察到的别名可归一化
6. 大小写归一化（L/LUM/Lum/lum/LUMINANCE 等）
7. Unicode 希腊字母 α（U+03B1）归一化
8. Unicode 下标 ₃（U+2083）和罗马数字 Ⅲ（U+2162）归一化
9. 全角字符归一化（ＯＩＩＩ）
10. 别名 -> 规范名 -> 别名 往返校验
11. 与 P10-001/P10-002/P10-003 观察到的别名交叉验证

## 输入与范围

- 测试数据：P10-004 生成的 FILTER_ALIAS_MAP.json + ALIAS_OBSERVATION_REPORT.json
- 测试脚本：`scripts/validate_filter_alias_map.py`（23 项测试）
- 测试方法：自动校验 + Unicode 字符测试 + 与 P10-001/P10-002/P10-003 交叉验证

## 执行/决策

### 测试矩阵

| 测试项 | 类型 | 必测项 | 状态 |
|--------|------|--------|------|
| T01 入口条件 (P10-001) | contract | 入口条件 | PASS |
| T02 入口条件 (P10-002) | contract | 入口条件 | PASS |
| T03 入口条件 (P10-003) | contract | 入口条件 | PASS |
| T04 FILTER_ALIAS_MAP.json 可解析 | unit | 对应测试 | PASS |
| T05 6 个规范名正确 | contract | 全部 | PASS |
| T06 禁止捷径: 6 规范名不同物理滤镜 | contract | 全部 | PASS |
| T07 alias_to_canonical 字段存在 | unit | 对应测试 | PASS |
| T08 canonical_to_aliases 覆盖 6 规范名 | unit | 对应测试 | PASS |
| T09 大小写归一化 (42 项) | unit | 对应测试 | PASS |
| T10 Unicode 希腊字母 α (4 项) | unit | 对应测试 | PASS |
| T11 Unicode 下标 ₃ + 罗马数字 Ⅲ (4 项) | unit | 对应测试 | PASS |
| T12 全角字符 ＯＩＩＩ 归一化 | unit | 对应测试 | PASS |
| T13 往返校验 (52 别名) | unit | 对应测试 | PASS |
| T14 canonical_to_preferred_alias 正确 | unit | 对应测试 | PASS |
| T15 canonical_to_doc_spelling 正确 | unit | 对应测试 | PASS |
| T16 与 P10-001 兼容 (22 旧别名) | unit | 旧功能回归 | PASS |
| T17 与 P10-002 profile filter_set 交叉验证 | unit | 真实数据测试 | PASS |
| T18 与 P10-003 inventory filter 交叉验证 | unit | 真实数据测试 | PASS |
| T19 observed_aliases_by_canonical 完整 | unit | 对应测试 | PASS |
| T20 禁止捷径: OIII 别名不合并 | contract | 全部 | PASS |
| T21 frozen_at 字段存在 | unit | 对应测试 | PASS |
| T22 frozen_by = P10-004 | unit | 对应测试 | PASS |
| T23 ALIAS_OBSERVATION_REPORT.json 可解析 | unit | 对应测试 | PASS |

### 测试详情

**T01-T03 入口条件**
- P10-001 交付物齐全：TESTDATA_EQUIPMENT_CATALOG.csv + TESTDATA_DATASET_CATALOG.csv + FILTER_ALIAS_MAP.json
- P10-002 交付物齐全：DEVICE_PROFILE_SUMMARY.json + T2/T3/T4_DEVICE_PROFILE.json
- P10-003 交付物齐全：CALIBRATION_MASTER_INVENTORY.csv
- PASS

**T04 JSON 可解析**
- FILTER_ALIAS_MAP.json 存在
- json.load 成功
- PASS

**T05 6 个规范名**
- _canonical_names = ['LUM', 'RED', 'GREEN', 'BLUE', 'HA', 'OIII']
- 与预期完全一致
- PASS

**T06 禁止捷径 - 6 规范名不同物理滤镜**
- _canonical_to_physical 包含 6 项
- 6 个物理描述全部唯一（无合并）
- LUM: Luminance 宽带, RED: Red 宽带 R, GREEN: Green 宽带 G, BLUE: Blue 宽带 B, HA: H-alpha 窄带 656.3nm, OIII: OIII 窄带 500.7nm
- PASS

**T07-T08 别名映射字段**
- alias_to_canonical: 52 个别名
- canonical_to_aliases: 覆盖全部 6 个规范名
- PASS

**T09 大小写归一化（42 项）**
- LUM 系列: L/l/LUM/Lum/lum/Luminance/luminance/LUMINANCE
- RED 系列: R/r/RED/Red/red
- GREEN 系列: G/g/GREEN/Green/green
- BLUE 系列: B/b/BLUE/Blue/blue
- HA 系列: H/h/HA/Ha/ha/H-alpha/h-alpha/H-Alpha/Halpha/halpha/HALPHA
- OIII 系列: OIII/Oiii/oiii/oIII/O3/o3/O-III/o-iii
- 全部 42 项归一化正确
- PASS

**T10 Unicode 希腊字母 α（4 项）**
- Hα (U+03B1) -> HA
- hα (U+03B1) -> HA
- HΑ (U+0391 大写希腊) -> HA
- α (U+03B1) -> HA
- PASS

**T11 Unicode 下标 ₃ + 罗马数字 Ⅲ（4 项）**
- O₃ (U+2083 下标 3) -> OIII
- o₃ (U+2083) -> OIII
- OⅢ (U+2162 罗马数字 III) -> OIII
- oⅢ (U+2162) -> OIII
- PASS

**T12 全角字符 ＯＩＩＩ**
- ＯＩＩＩ (U+FF2F U+FF29 U+FF29 U+FF29) -> OIII
- PASS

**T13 往返校验（52 别名）**
- 每个别名 -> 规范名 -> canonical_to_aliases[规范名] 列表包含原别名
- 52 项全部通过
- PASS

**T14-T15 首选别名与文档拼写**
- canonical_to_preferred_alias: {LUM:Lum, RED:Red, GREEN:Green, BLUE:Blue, HA:H-alpha, OIII:OIII}
- canonical_to_doc_spelling: {LUM:Lum, RED:Red, GREEN:Green, BLUE:Blue, HA:Halpha, OIII:OIII}
- 与预期完全一致
- PASS

**T16 与 P10-001 兼容性**
- P10-001 FILTER_ALIAS_MAP.json 的 _canonical_to_aliases 中 22 个别名
- 全部能用 P10-004 的 normalize_alias 归一化
- 旧功能回归无退化
- PASS

**T17 与 P10-002 profile filter_set 交叉验证**
- T2/T3/T4 DEVICE_PROFILE.json 的 filter_set 共 7 个唯一别名
- 全部可归一化
- PASS

**T18 与 P10-003 inventory filter 交叉验证**
- CALIBRATION_MASTER_INVENTORY.csv 的 filter_from_filename + filter_from_header 共 7 个唯一别名
- 全部可归一化
- PASS

**T19 observed_aliases_by_canonical 完整**
- 6 个规范名都有非空观察别名列表
- 共 11 个观察别名（部分跨规范名共享）
- PASS

**T20 禁止捷径 - OIII 别名不合并**
- OIII/Oiii/oiii/O3/O-III/O₃/OⅢ 共 7 个变体
- 全部映射到规范名 OIII（不合并到其他规范名）
- PASS

**T21-T22 frozen_at / frozen_by**
- frozen_at: ISO 8601 时间戳 (2026-07-27T...)
- frozen_by: "P10-004"
- PASS

**T23 ALIAS_OBSERVATION_REPORT.json 可解析**
- JSON 可解析
- 11 个观察别名
- 0 个 unresolved
- PASS

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python freeze_filter_alias_map.py` | 60s | 0 |
| `python validate_filter_alias_map.py` | 60s | 0 |

## 结果与证据

- **23/23 测试 PASS**（详见 raw_logs/validate_filter_alias_map.log）
- 2 个交付物完整（FILTER_ALIAS_MAP.json + ALIAS_OBSERVATION_REPORT.json）
- 6 个规范名对应 6 个不同物理滤镜（禁止合并 PASS）
- 52 个别名全部可归一化（含 Unicode/全角/大小写变体）
- 与 P10-001/P10-002/P10-003 观察到的别名交叉验证全部可归一化
- 旧功能回归无退化（P10-001 的 22 个别名全部兼容）

## 风险/回滚/残留

- 41 个别名是预防性扩展（Unicode/全角/大小写变体），实际观察到 11 个
- 文档滤镜行格式不统一（7 种变体），regex 提取已覆盖
- OIII/Oiii 别名冲突已解决（统一映射到 OIII）

## 结论

P10-004 测试全部通过。23/23 测试 PASS。6 个规范名（LUM/RED/GREEN/BLUE/HA/OIII）冻结，52 个别名（含 Unicode 希腊字母 α、下标 ₃、罗马数字 Ⅲ、全角字符）全部可归一化。禁止捷径检查通过（6 个规范名对应 6 个不同物理滤镜，无合并）。与 P10-001/P10-002/P10-003 观察到的 11 个别名交叉验证全部可归一化。OIII/Oiii 别名冲突已解决。
