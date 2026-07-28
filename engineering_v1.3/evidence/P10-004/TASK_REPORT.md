# 任务报告

- Task/ADR：P10-004 冻结滤镜规范名和别名
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

依据 `tasks/P10-004.md` 和 `docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`，从说明文档 / FITS+XISF Header / 文件名 三个来源建立规范名与别名的最终冻结映射，测试 Unicode/大小写变体。

P10-001 阶段已生成初版 `FILTER_ALIAS_MAP.json`（仅含观察到的 7 个别名），本任务将其扩展为完整的 52 别名映射表（含 Unicode 希腊字母 α、下标 ₃、罗马数字 Ⅲ、全角字符等变体），并冻结。

## 输入与范围

- 数据源 1：testdata/**/*.txt（7 个素材信息文档，含滤镜清单）
- 数据源 2：P10-001/raw_logs/header_samples.json（49 FITS + 27 XISF Header）
- 数据源 3：P10-001/TESTDATA_DATASET_CATALOG.csv（49 数据集 filter_in_header / filter_in_filename）
- 数据源 4：P10-002/T2-T4_DEVICE_PROFILE.json（filter_set / filter_set_doc）
- 数据源 5：P10-003/CALIBRATION_MASTER_INVENTORY.csv（27 文件 filter_from_filename / filter_from_header）
- 依赖：P10-001 + P10-002（已满足）
- 参考规范：`docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`
- 工具：`engineering_v1.2/evidence/P10-004/scripts/freeze_filter_alias_map.py`

## 执行/决策

### 阶段 1：扫描所有数据源的滤镜名

| 数据源 | 观察到的别名 |
|--------|-------------|
| 文档 .txt（7 个） | L, R, G, B, H, Ha, Halpha, Lum, Red, Green, Blue, OIII |
| FITS + XISF Header | Blue, Green, H-alpha, Lum, OIII, Oiii, Red |
| TESTDATA_DATASET_CATALOG（filter_in_header） | Blue, Green, H-alpha, Lum, OIII, Oiii, Red |
| TESTDATA_DATASET_CATALOG（filter_in_filename） | Blue, Green, H-alpha, Lum, OIII, Oiii, Red |
| CALIBRATION_MASTER_INVENTORY（filter_from_filename） | Blue, Green, Lum, OIII, Oiii, Red |
| CALIBRATION_MASTER_INVENTORY（filter_from_header） | Blue, Green, H-alpha, Lum, OIII, Oiii, Red |
| DEVICE_PROFILE（filter_set，Header） | Blue, Green, H-alpha, Lum, OIII, Oiii, Red |
| DEVICE_PROFILE（filter_set_doc，文档） | L, R, G, B |

观察到的别名全集（去重）：{B, Blue, G, Green, H-alpha, L, Lum, OIII, Oiii, R, Red}（共 11 个）

### 阶段 2：构建完整别名映射表

冻结 6 个规范名（硬门限，不允许产生新规范名）：
- **LUM**（Luminance 宽带滤镜，透过整个可见光谱）
- **RED**（Red 宽带 R 滤镜）
- **GREEN**（Green 宽带 G 滤镜）
- **BLUE**（Blue 宽带 B 滤镜）
- **HA**（H-alpha 窄带滤镜，656.3nm 氢α发射线）
- **OIII**（OIII 窄带滤镜，500.7nm 双电离氧发射线）

构建 `ALIAS_TO_CANONICAL` 字典，共 **52 个别名**：
- 大小写变体（L/LUM/Lum/lum/LUMINANCE/Luminance/luminance 等）
- Unicode 希腊字母 α（U+03B1）：Hα, hα, HΑ（大写希腊）, α
- Unicode 下标 ₃（U+2083）：O₃, o₃
- Unicode 罗马数字 Ⅲ（U+2162）：OⅢ, oⅢ
- 全角字符：ＯＩＩＩ（U+FF2F U+FF29 U+FF29 U+FF29）
- 连字符/分隔变体：H-alpha/H-Alpha/h-alpha, O-III/o-iii
- 数字变体：O3/o3

### 阶段 3：归一化函数实现

```python
def normalize_alias(alias: str) -> str | None:
    """将别名归一化为规范名, 失败返回 None."""
    if not alias: return None
    key = alias.strip()
    if not key: return None
    if key in ALIAS_TO_CANONICAL:
        return ALIAS_TO_CANONICAL[key]
    upper = key.upper()
    if upper in ALIAS_TO_CANONICAL:
        return ALIAS_TO_CANONICAL[upper]
    return None
```

策略：
1. 先 strip 后直接查表（覆盖 Unicode 字符）
2. 大小写归一化后查表（覆盖大小写变体）
3. 失败返回 None

### 阶段 4：禁止捷径检查

每个规范名对应不同的物理滤镜（不允许合并不同物理滤镜）：
- LUM ≠ RED ≠ GREEN ≠ BLUE ≠ HA ≠ OIII（6 个独立物理滤镜）
- 6 个物理描述全部唯一（无重复）
- 关键冲突解决：OIII（T2 Header）与 Oiii（T3/T4 Header）是同一物理滤镜的不同大小写 → 全部映射到 OIII

### 阶段 5：构建 FILTER_ALIAS_MAP.json

输出文件包含：
- `_canonical_names`：6 个规范名
- `_canonical_to_physical`：规范名 -> 物理滤镜描述
- `_canonical_to_preferred_alias`：规范名 -> 首选别名（Header 实际观察值）
- `_canonical_to_doc_spelling`：规范名 -> 文档拼写
- `canonical_to_aliases`：规范名 -> 别名列表
- `alias_to_canonical`：别名 -> 规范名（52 项）
- `observed_aliases_by_canonical`：按规范名分组的实际观察别名
- `_observed_aliases_by_source`：按数据源分组的观察别名
- `frozen_at` / `frozen_by`

### 阶段 6：构建 ALIAS_OBSERVATION_REPORT.json

记录每个别名的来源（文档/Header/filename/device profile），便于追溯。

### 阶段 7：运行验证脚本

`validate_filter_alias_map.py` 执行 23 项测试，全部 PASS。

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python freeze_filter_alias_map.py` | 60s | 0 |
| `python validate_filter_alias_map.py` | 60s | 0 |

## 结果与证据

### 交付物

1. **FILTER_ALIAS_MAP.json** — 52 个别名映射到 6 个规范名
2. **ALIAS_OBSERVATION_REPORT.json** — 11 个观察别名的来源报告

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

### 规范名 -> 首选别名 / 文档拼写

| 规范名 | 首选别名（Header） | 文档拼写 | 物理滤镜 |
|--------|-------------------|---------|---------|
| LUM | Lum | Lum | Luminance 宽带 |
| RED | Red | Red | Red 宽带 R |
| GREEN | Green | Green | Green 宽带 G |
| BLUE | Blue | Blue | Blue 宽带 B |
| HA | H-alpha | Halpha | H-alpha 窄带 656.3nm |
| OIII | OIII | OIII | OIII 窄带 500.7nm |

### 关键发现

1. **OIII 别名冲突解决**：T2 Header 用 "OIII"，T3/T4 Header 用 "Oiii"，文档用 "OIII" — 全部映射到规范名 OIII
2. **H-alpha 拼写不一致**：Header 用 "H-alpha"，文档用 "Halpha" 或 "Ha" — 全部映射到规范名 HA
3. **文档拼写变体**：6 个素材文档中滤镜行格式不统一（"L,R,G,B, 3nmHalpha"、"LRGBHO 50mm"、"RGBHaOIII" 等），需用 regex 提取
4. **Unicode 测试覆盖**：希腊字母 α（U+03B1）、下标 ₃（U+2083）、罗马数字 Ⅲ（U+2162）、全角字符 ＯＩＩＩ 全部归一化成功
5. **大小写归一化**：L/l/LUM/Lum/lum 全部映射到 LUM，符合预期
6. **无未声明 fallback**：所有 11 个观察到的别名都能归一化，无未映射项
7. **禁止捷径检查通过**：6 个规范名对应 6 个不同物理滤镜，无合并

## 风险/回滚/残留

- **OIII/Oiii 别名不一致**：本任务已统一到规范名 OIII，P10-005 解析时使用规范名匹配
- **H-alpha 拼写不一致**：本任务已统一到规范名 HA，P10-005 解析时使用规范名匹配
- **文档滤镜行格式不统一**：freeze 脚本用 regex 提取，覆盖 7 种格式变体
- **未观察到的别名**：52 个别名中 41 个是预防性扩展（Unicode/全角/大小写变体），实际观察到 11 个

## 结论

P10-004 完成。2 个交付物已生成（FILTER_ALIAS_MAP.json + ALIAS_OBSERVATION_REPORT.json）。6 个规范名（LUM/RED/GREEN/BLUE/HA/OIII）冻结，52 个别名（含 Unicode 希腊字母/下标/罗马数字/全角字符）全部可归一化。23/23 测试 PASS。禁止捷径检查通过（6 个规范名对应 6 个不同物理滤镜，无合并）。与 P10-001/P10-002/P10-003 观察到的 11 个别名交叉验证全部可归一化。OIII/Oiii 别名冲突已解决（统一映射到 OIII）。
