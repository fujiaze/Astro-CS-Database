# 复核报告

- Task/ADR：P10-004 冻结滤镜规范名和别名
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

独立复核 P10-004 是否满足任务定义 `tasks/P10-004.md` 和规范 `docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md` 的全部要求。

## 输入与范围

- 任务定义：tasks/P10-004.md
- 规范：docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md
- 交付物：FILTER_ALIAS_MAP.json + ALIAS_OBSERVATION_REPORT.json
- 脚本：scripts/freeze_filter_alias_map.py + scripts/validate_filter_alias_map.py
- 原始日志：raw_logs/

## 执行/决策

### 复核矩阵

| 复核项 | 验证内容 | 结果 |
|--------|----------|------|
| R-01 任务要求覆盖 | 任务定义的全部要求是否满足 | PASS |
| R-02 交付物完整性 | 2 个交付物是否全部生成 | PASS |
| R-03 6 个规范名正确 | LUM/RED/GREEN/BLUE/HA/OIII | PASS |
| R-04 禁止捷径: 不同物理滤镜不合并 | 6 个规范名对应 6 个不同物理描述 | PASS |
| R-05 三来源数据扫描完整 | 文档/Header/文件名 全部扫描 | PASS |
| R-06 观察到的别名全部可归一化 | 11 个观察别名 0 个 unresolved | PASS |
| R-07 Unicode 希腊字母 α 归一化 | Hα/hα/HΑ/α 全部映射到 HA | PASS |
| R-08 Unicode 下标 ₃ + 罗马数字 Ⅲ | O₃/o₃/OⅢ/oⅢ 全部映射到 OIII | PASS |
| R-09 大小写归一化 | 42 项大小写变体全部归一化 | PASS |
| R-10 全角字符归一化 | ＯＩＩＩ 映射到 OIII | PASS |
| R-11 往返校验 | 52 别名 -> 规范名 -> 别名列表 | PASS |
| R-12 与 P10-001 兼容性 | 22 个旧别名全部可归一化 | PASS |
| R-13 与 P10-002/P10-003 交叉验证 | 7+7 个观察别名全部可归一化 | PASS |
| R-14 测试覆盖与通过率 | 23/23 测试 PASS | PASS |
| R-15 禁止捷径检查 | 无未声明 fallback, 无未映射别名 | PASS |

### R-01 任务要求覆盖

任务定义要求：
1. ✅ 从说明文档/Header/文件名建立规范映射并测试 Unicode/大小写
   - 文档扫描：7 个 .txt 文件，提取 L/R/G/B/Ha/Halpha/Lum/Red/Green/Blue/OIII
   - Header 扫描：49 FITS + 27 XISF Header，提取 Blue/Green/H-alpha/Lum/OIII/Oiii/Red
   - 文件名扫描：CALIBRATION_MASTER_INVENTORY.csv 的 filter_from_filename + filter_from_header
   - Unicode 测试：希腊字母 α、下标 ₃、罗马数字 Ⅲ、全角字符 全部覆盖
   - 大小写测试：42 项大小写变体
2. ✅ 输出 FILTER_ALIAS_MAP.json + alias tests
   - FILTER_ALIAS_MAP.json 已冻结（52 别名）
   - 验证脚本 validate_filter_alias_map.py 包含 23 项测试

规范要求（docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md）：
- ✅ FILTER_ALIAS_MAP.json 是规范要求的交付物之一
- ✅ 硬门限：只允许 T1-T4 四套规范设备 ID — 本任务不涉及设备 ID，但所有滤镜均来自 T2/T3/T4 设备（T1 无数据）

### R-02 交付物完整性

| 交付物 | 状态 | 内容 |
|--------|------|------|
| FILTER_ALIAS_MAP.json | PASS | 52 别名映射到 6 规范名, 含 Unicode/全角/大小写变体 |
| ALIAS_OBSERVATION_REPORT.json | PASS | 11 个观察别名的来源报告, 0 unresolved |

### R-03 6 个规范名正确

- _canonical_names = ['LUM', 'RED', 'GREEN', 'BLUE', 'HA', 'OIII']
- 与任务要求一致（6 个物理滤镜：LRGB+Ha+OIII）
- PASS

### R-04 禁止捷径 - 不同物理滤镜不合并

| 规范名 | 物理滤镜描述 | 唯一性 |
|--------|-------------|--------|
| LUM | Luminance 宽带滤镜 (透过整个可见光谱) | 唯一 |
| RED | Red 宽带 R 滤镜 | 唯一 |
| GREEN | Green 宽带 G 滤镜 | 唯一 |
| BLUE | Blue 宽带 B 滤镜 | 唯一 |
| HA | H-alpha 窄带滤镜 (656.3nm 氢α发射线) | 唯一 |
| OIII | OIII 窄带滤镜 (500.7nm 双电离氧发射线) | 唯一 |

6 个物理描述全部唯一，无合并。PASS。

### R-05 三来源数据扫描完整

| 数据源 | 扫描内容 | 结果 |
|--------|---------|------|
| 文档 .txt (7 个) | 滤镜行 + 曝光时间行的滤镜 token | 11 个别名 (L/R/G/B/Ha/Halpha/Lum/Red/Green/Blue/OIII) |
| FITS Header (49 个) | filter 字段 | 7 个别名 |
| XISF Header (27 个) | filter 字段 | 7 个别名 |
| 文件名 (49 light + 27 calib) | filter_from_filename | 7 个别名 |
| Device Profile (4 个) | filter_set + filter_set_doc | 11 个别名 |

观察到的别名全集（去重）：{B, Blue, G, Green, H-alpha, L, Lum, OIII, Oiii, R, Red}（11 个）
PASS。

### R-06 观察到的别名全部可归一化

11 个观察别名全部能用 normalize_alias 归一化：
- L -> LUM, R -> RED, G -> GREEN, B -> BLUE
- Lum -> LUM, Red -> RED, Green -> GREEN, Blue -> BLUE
- H-alpha -> HA
- OIII -> OIII, Oiii -> OIII

0 个 unresolved。PASS。

### R-07 Unicode 希腊字母 α 归一化

| 别名 | Unicode | 规范名 |
|------|---------|--------|
| Hα | U+03B1 | HA |
| hα | U+03B1 | HA |
| HΑ | U+0391 (大写希腊) | HA |
| α | U+03B1 | HA |

4/4 PASS。

### R-08 Unicode 下标 ₃ + 罗马数字 Ⅲ

| 别名 | Unicode | 规范名 |
|------|---------|--------|
| O₃ | U+2083 | OIII |
| o₃ | U+2083 | OIII |
| OⅢ | U+2162 | OIII |
| oⅢ | U+2162 | OIII |

4/4 PASS。

### R-09 大小写归一化

42 项大小写变体测试全部通过：
- LUM: L/l/LUM/Lum/lum/Luminance/luminance/LUMINANCE
- RED: R/r/RED/Red/red
- GREEN: G/g/GREEN/Green/green
- BLUE: B/b/BLUE/Blue/blue
- HA: H/h/HA/Ha/ha/H-alpha/h-alpha/H-Alpha/Halpha/halpha/HALPHA
- OIII: OIII/Oiii/oiii/oIII/O3/o3/O-III/o-iii

PASS。

### R-10 全角字符归一化

- ＯＩＩＩ (U+FF2F U+FF29 U+FF29 U+FF29) -> OIII
- PASS

### R-11 往返校验

52 个别名 -> 规范名 -> canonical_to_aliases[规范名] 列表 -> 包含原别名：
- 52/52 全部通过
- 无遗漏
- PASS

### R-12 与 P10-001 兼容性

P10-001 FILTER_ALIAS_MAP.json 的 _canonical_to_aliases 中 22 个别名：
- L, LUM, Lum, Luminance
- R, RED, Red
- G, GREEN, Green
- B, BLUE, Blue
- H-Alpha, H-alpha, HA, Ha, Halpha
- O-III, O3, OIII, Oiii

全部能用 P10-004 的 normalize_alias 归一化。22/22 PASS。旧功能回归无退化。

### R-13 与 P10-002/P10-003 交叉验证

| 数据源 | 观察别名 | 可归一化 |
|--------|---------|---------|
| P10-002 T2/T3/T4 device profile filter_set | 7 (Blue, Green, H-alpha, Lum, OIII, Oiii, Red) | 7/7 |
| P10-003 CALIBRATION_MASTER_INVENTORY filter | 7 (Blue, Green, H-alpha, Lum, OIII, Oiii, Red) | 7/7 |

PASS。

### R-14 测试覆盖与通过率

- 测试脚本：scripts/validate_filter_alias_map.py
- 测试点数：23（覆盖 contract/unit/真实数据/Unicode/旧功能回归/禁止捷径）
- 通过率：23/23 (100%)
- 失败数：0
- 跳过数：0
- PASS

### R-15 禁止捷径检查

- 无未声明 fallback ✅
- 无未映射别名（11 个观察别名全部可归一化）✅
- 无数据范围缩减（52 个别名覆盖全部 11 个观察 + 41 个预防性扩展）✅
- 6 个规范名对应 6 个不同物理滤镜（无合并）✅
- OIII/Oiii 别名冲突已解决（统一映射到 OIII，不合并到其他规范名）✅
- PASS

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python freeze_filter_alias_map.py` | 60s | 0 |
| `python validate_filter_alias_map.py` | 60s | 0 |

## 结果与证据

- 15/15 复核项 PASS
- 2 个交付物完整（FILTER_ALIAS_MAP.json + ALIAS_OBSERVATION_REPORT.json）
- 23/23 测试通过
- 6 个规范名对应 6 个不同物理滤镜（禁止合并 PASS）
- 52 个别名全部可归一化（含 Unicode 希腊字母 α、下标 ₃、罗马数字 Ⅲ、全角字符）
- 与 P10-001/P10-002/P10-003 观察到的 11 个别名交叉验证全部可归一化
- 旧功能回归无退化（P10-001 的 22 个别名全部兼容）
- 禁止捷径检查通过

## 风险/回滚/残留

- 41 个别名是预防性扩展（Unicode/全角/大小写变体），实际观察到 11 个
- 文档滤镜行格式不统一（7 种变体），regex 提取已覆盖
- OIII/Oiii 别名冲突已解决（统一映射到 OIII）
- H-alpha 拼写不一致已解决（统一映射到 HA）

## 结论

P10-004 独立复核通过。15 项复核全部 PASS。2 个交付物完整（FILTER_ALIAS_MAP.json 52 别名 + ALIAS_OBSERVATION_REPORT.json），23/23 测试通过，硬门限满足。6 个规范名（LUM/RED/GREEN/BLUE/HA/OIII）对应 6 个不同物理滤镜（禁止合并 PASS）。52 个别名全部可归一化，含 Unicode 希腊字母 α、下标 ₃、罗马数字 Ⅲ、全角字符。与 P10-001/P10-002/P10-003 观察到的 11 个别名交叉验证全部可归一化。旧功能回归无退化。OIII/Oiii 别名冲突已解决。

VERDICT: PASS
