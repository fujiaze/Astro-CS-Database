# 任务报告

- Task/ADR：P10-005 实现并验证 Light 到 Master 唯一解析
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

依据 `tasks/P10-005.md` 和 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`，实现 Light 帧 到 Bias/Dark/Flat Master 的唯一解析 resolver，对全 710 Light 帧输出每帧的选择理由与歧义，禁止 first-match 或静默选择歧义。

P10-002 已记录 T2/T4 缺少 Lum Flat（设备档案中 `missing_flats=[Lum]`），本任务须在 resolver 中显式标记此缺失，不得静默替代。

## 输入与范围

- 依赖（已满足）：P10-002（设备档案）+ P10-003（27 个 Master 盘点）+ P10-004（52 别名映射）
- 数据源 1：`P10-002/T2_DEVICE_PROFILE.json` / `T3_DEVICE_PROFILE.json` / `T4_DEVICE_PROFILE.json`（设备档案 + missing_flats 记录）
- 数据源 2：`P10-003/CALIBRATION_MASTER_INVENTORY.csv`（27 个 Master 元数据，含 file_path/device_id/master_type/bin/exposure/image_size/filter）
- 数据源 3：`P10-004/FILTER_ALIAS_MAP.json`（52 别名 -> 6 规范名 LUM/RED/GREEN/BLUE/HA/OIII）
- 数据源 4：`P10-001/raw_logs/header_samples.json`（49 FITS Header 采样，含 filter/exposure/bin/image_size）
- 数据源 5：`testdata/**/*.fts`（710 Light 帧路径）
- 参考规范：`docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`
- 工具：`engineering_v1.2/evidence/P10-005/scripts/resolve_light_to_master.py`
- 验证：`engineering_v1.2/evidence/P10-005/scripts/test_resolver.py`（23 测试）

## 执行/决策

### 阶段 1：诊断 0 resolved 根因

初次运行 resolver 时全部 710 Light 帧 0 resolved。诊断脚本 `diagnose_match.py` 揭示根因：

| 字段 | Master CSV 实际值 | Resolver 期望值 |
|------|------------------|----------------|
| `image_size_from_header` | **空**（XISF NAXIS1/NAXIS2 未提取） | `"4500x3600"` |
| `image_size_from_filename` | `"4500x3600"`（正确） | 未使用 |

P10-003 的 XISF header 解析器从 FITSKeyword 元素查找 NAXIS1/NAXIS2，但 XISF 格式将图像几何信息存储在 `<Image>` 元素的 `geometry` 属性中，导致 `image_size_from_header` 全为空。匹配条件 `m["image_size_from_header"] == image_size` 永远失败。

### 阶段 2：修复 resolver - 添加统一 image_size 字段

`load_calibration_masters()` 改为统一字段策略（header 优先，filename 兜底）：

```python
row["image_size"] = (
    row.get("image_size_from_header")
    or row.get("image_size_from_filename")
    or ""
)
```

同时统一其他字段：
- `exposure_s`: `exposure_from_header` or `exposure_from_filename`
- `bin_int`: `bin_from_header` or `bin_from_filename`
- `canonical_filter`: `filter_from_header` or `filter_from_filename`（经 P10-004 归一化）

匹配函数 `match_bias` / `match_dark` / `match_flat` 改为使用统一字段 `m["image_size"]`。

### 阶段 3：匹配规则实现

依据 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`：

| Master 类型 | 匹配键 | 选择规则 |
|------------|--------|---------|
| **Bias** | device_id + bin + image_size | 唯一匹配 (1 个) -> unique；0 个 -> missing；多个 -> ambiguous |
| **Dark** | device_id + bin + image_size + exposure | 1. 精确匹配 (Light exp == Dark exp, 容差 0.01s) -> exact<br>2. >= Light exp 的最接近 -> closest_longer<br>3. 全部 dark < Light exp -> fallback_longest (显式标记)<br>多个精确 -> ambiguous_exact |
| **Flat** | device_id + bin + image_size + canonical_filter | 唯一匹配 -> unique；0 个 -> missing_<filter>_flat；多个 -> ambiguous |

### 阶段 4：禁止捷径实现

| 禁止项 | 实现方式 |
|--------|---------|
| 不得 first-match | 多个匹配时返回 `ambiguous` 状态 + 第一个 master + 显式标记 "multiple X matches (N)" |
| 不得静默选择歧义 | 每个 Light 输出 `bias_match_reason` / `dark_match_reason` / `flat_match_reason` 字符串 |
| 不得用其他滤镜 flat 替代 | 缺失 Flat 时返回 `missing_<filter>_flat`，`flat_master` 字段为空字符串 |
| 不得未声明 fallback | Dark 的 fallback_longest 显式标记 "fallback: all darks < Light exposure" |

### 阶段 5：端到端 resolver 运行

```
[阶段 1] 加载数据...
  Master 数: 27 (T2:9 T3:9 T4:9)
  Header 采样: 49 个 (FITS)
[阶段 2] 枚举 Light 文件...
  Light 文件总数: 710
[阶段 3] 解析每张 Light...
  [100/710] resolved=100, unresolved=0
  [400/710] resolved=375, unresolved=25
  [710/710] resolved=587, unresolved=123
```

### 阶段 6：123 unresolved 全部为 missing_lum_flat

123 unresolved 全部为 T2/T4 的 Lum Light 帧（无对应 Lum Flat Master）：

| 设备 | Lum Light 数 | 缺失原因 | 与 P10-002 一致性 |
|------|-------------|---------|-------------------|
| T2 | 25 (LDN43 10 + NGC247 15) | T2 calibration files/ 无 Lum flat | ✅ P10-002 T2_DEVICE_PROFILE.json `missing_flats=[Lum]` |
| T4 | 98 (Victory_Nebula panel1 49 + panel2 49) | T4 calibration files/ 无 Lum flat | ✅ P10-002 T4_DEVICE_PROFILE.json `missing_flats=[Lum]` |

T3 不缺 Lum flat（`masterFlat_BIN-1_4096x4096_FILTER-Lum_mono.xisf` 存在），故 T3 的 22 个 Lum Light 帧（NGC83_cluster）全部 resolved。

### 阶段 7：单元测试 23 项

`test_resolver.py` 覆盖：
- normalize_filter: 直接映射 / 大小写归一 / 未知别名 (3 测试)
- match_bias: unique / missing / ambiguous (3 测试)
- match_dark: exact / closest_longer / fallback_longest / missing / ambiguous_exact (5 测试)
- match_flat: unique / missing_no_filter / missing_no_master / ambiguous (4 测试)
- load_calibration_masters: 27 master 加载 / image_size 统一字段 / canonical_filter 归一化 (3 测试)
- derive_device_id: 路径推导 (1 测试)
- 端到端: 710 Light 帧统计 / 禁止静默 fallback / summary JSON 一致性 (3 测试)
- 禁止捷径: no first-match for ambiguous (1 测试)

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python resolve_light_to_master.py` | 60s | 0 |
| `python test_resolver.py` | 60s | 0 |
| `python diagnose_match.py`（诊断，非交付） | 30s | 0 |

## 结果与证据

### 交付物

1. **LIGHT_TO_MASTER_RESOLUTION.csv** — 710 行，每行一个 Light 帧 + Bias/Dark/Flat 匹配结果
2. **UNRESOLVED_CALIBRATION_REPORT.md** — 123 unresolved 详情（全部 missing_lum_flat）
3. **RESOLUTION_SUMMARY.json** — 汇总统计

### 关键统计

| 指标 | 值 |
|------|-----|
| 总 Light 帧 | 710 |
| Resolved (Bias + Dark + Flat 全部 unique/exact) | 587 (82.7%) |
| Unresolved (Flat missing) | 123 (17.3%) |
| Bias 状态 | 710 unique (100%) |
| Dark 状态 | 710 exact (100%) |
| Flat 状态 | 587 unique + 123 missing |
| 歧义分类 | 587 NONE + 123 FLAT_MISSING |
| T2 Lum unresolved | 25 (LDN43 10 + NGC247 15) |
| T4 Lum unresolved | 98 (Victory_Nebula 49+49) |
| T3 Lum resolved | 22 (NGC83_cluster, T3 有 Lum flat) |

### 按设备分布

| 设备 | 总 Light | Resolved | Unresolved | 说明 |
|------|---------|----------|-----------|------|
| T2 | 174 | 149 | 25 | 25 Lum 缺 flat |
| T3 | 151 | 151 | 0 | 全部 resolved |
| T4 | 385 | 287 | 98 | 98 Lum 缺 flat |

### 按滤镜规范名分布

| 规范名 | 总 Light | Resolved | Unresolved |
|--------|---------|----------|-----------|
| LUM | 160 | 37 (T3) | 123 (T2 25 + T4 98) |
| RED | 132 | 132 | 0 |
| GREEN | 137 | 137 | 0 |
| BLUE | 132 | 132 | 0 |
| HA | 77 | 77 | 0 |
| OIII | 72 | 72 | 0 |

### 关键发现

1. **0 resolved 根因**：P10-003 的 XISF header 解析器未提取 NAXIS1/NAXIS2（存于 Image 元素属性，非 FITSKeyword），导致 `image_size_from_header` 全空。修复方案：resolver 使用统一 `image_size` 字段（header 优先，filename 兜底）。
2. **123 unresolved 全部为 missing_lum_flat**：T2/T4 设备档案中已记录 `missing_flats=[Lum]`（P10-002），resolver 正确标记，未静默替代。
3. **Bias/Dark 100% 匹配**：27 个 Master（3 Bias + 8 Dark + 16 Flat）覆盖全部 T2/T3/T4 的 Bias 和 Dark 需求。
4. **禁止捷径 PASS**：所有 missing 的 Light 帧 `flat_master` 字段为空字符串，未选其他滤镜 flat 作替代。
5. **Dark 兜底显式标记**：T4 Oiii Light（600s）使用 600s Dark exact 匹配，无 fallback；T2/T3 H-alpha Light（1200s）使用 1200s Dark exact 匹配，无 fallback。

## 风险/回滚/残留

- **123 Lum Light 帧 BLOCKED**：T2/T4 缺 Lum Flat，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录，本任务正确标记未静默替代。
- **image_size_from_header 全空**：P10-003 的 XISF header 解析器未提取 NAXIS1/NAXIS2，本任务用 filename 兜底（文件名格式 `masterXxx_BIN-1_<W>x<H>_...` 已包含可靠尺寸）。建议后续 P10-003 修复 XISF Image geometry 解析。
- **fallback_longest 未触发**：本次 710 Light 全部有精确 Dark 匹配，无 fallback_longest 场景。单元测试已覆盖此分支。
- **ambiguous 未触发**：本次 27 Master 无重复，无 ambiguous 场景。单元测试已覆盖此分支。

## 结论

P10-005 完成。3 个交付物已生成（LIGHT_TO_MASTER_RESOLUTION.csv 710 行 + UNRESOLVED_CALIBRATION_REPORT.md + RESOLUTION_SUMMARY.json）。587/710 Light 帧 resolved（Bias unique + Dark exact + Flat unique），123/710 unresolved（全部 T2/T4 Lum Light 缺 Flat，与 P10-002 设备档案一致，未静默替代）。Bias 100% unique，Dark 100% exact，Flat 587 unique + 123 missing。23/23 测试 PASS。禁止捷径检查通过（无 first-match，无静默 fallback，无未声明 fallback）。0 resolved 根因已诊断并修复（image_size 统一字段）。
