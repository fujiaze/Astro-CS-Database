# 测试报告

- Task/ADR：P10-005 实现并验证 Light 到 Master 唯一解析
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

验证 P10-005 resolver 的正确性：
1. Bias/Dark/Flat 三类 Master 的匹配规则（unique/missing/ambiguous/exact/closest_longer/fallback_longest）
2. 全 710 Light 帧端到端解析结果（587 resolved + 123 unresolved missing_lum_flat）
3. 禁止捷径：不得 first-match，不得静默选择歧义，不得用其他滤镜 flat 替代
4. 与 P10-002 设备档案中 `missing_flats=[Lum]` 一致性

## 输入与范围

- 测试脚本：`engineering_v1.2/evidence/P10-005/scripts/test_resolver.py`
- 测试对象：`engineering_v1.2/evidence/P10-005/scripts/resolve_light_to_master.py`
- 测试数据：
  - P10-004 FILTER_ALIAS_MAP.json（52 别名映射）
  - P10-003 CALIBRATION_MASTER_INVENTORY.csv（27 master）
  - P10-001 header_samples.json（49 FITS Header）
  - testdata/**/*.fts（710 Light 帧）
- 测试覆盖：23 项（unit + contract + e2e + forbidden shortcut）
- 测试框架：Python assert + 自定义 main() runner

## 执行/决策

### 测试矩阵

| # | 测试名 | 类型 | 验证内容 | 结果 |
|---|--------|------|---------|------|
| 1 | test_normalize_filter_direct_mapping | unit | 6 规范名直接映射 (Lum->LUM, Red->RED, ...) | PASS |
| 2 | test_normalize_filter_case_insensitive | unit | 大小写归一 (LUM/lum/Lum) | PASS |
| 3 | test_normalize_filter_unknown | unit | 未知别名返回 None | PASS |
| 4 | test_match_bias_unique | unit | Bias 唯一匹配 | PASS |
| 5 | test_match_bias_missing | unit | Bias 缺失 | PASS |
| 6 | test_match_bias_ambiguous | unit | Bias 歧义 (多个匹配标记 ambiguous) | PASS |
| 7 | test_match_dark_exact | unit | Dark 精确匹配 (Light exp == Dark exp) | PASS |
| 8 | test_match_dark_closest_longer | unit | Dark 选择 >= Light exp 的最接近 | PASS |
| 9 | test_match_dark_fallback_longest | unit | Dark 兜底 (全部 dark < Light exp) | PASS |
| 10 | test_match_dark_missing | unit | Dark 缺失 | PASS |
| 11 | test_match_dark_ambiguous_exact | unit | Dark 歧义 (多个精确匹配) | PASS |
| 12 | test_match_flat_unique | unit | Flat 唯一匹配 (设备+bin+size+filter) | PASS |
| 13 | test_match_flat_missing_no_filter | unit | Flat 缺失 (Light 无 canonical filter) | PASS |
| 14 | test_match_flat_missing_no_master | unit | Flat 缺失 (无对应滤镜 master, 如 T2 缺 Lum) | PASS |
| 15 | test_match_flat_ambiguous | unit | Flat 歧义 (多个匹配) | PASS |
| 16 | test_load_masters_count | contract | 加载 27 个 master | PASS |
| 17 | test_load_masters_image_size_unified | contract | image_size 统一字段 (header 优先, filename 兜底) | PASS |
| 18 | test_load_masters_canonical_filter | contract | canonical_filter 归一化 (Lum->LUM, H-alpha->HA, Oiii->OIII) | PASS |
| 19 | test_derive_device_id | unit | 从路径推导设备 ID (T2/T3/T4) | PASS |
| 20 | test_e2e_resolution_csv | e2e | 710 Light 帧端到端统计 (resolved=587, unresolved=123) | PASS |
| 21 | test_e2e_no_silent_fallback | e2e | 禁止静默 fallback (unresolved 的 flat_master 必须为空) | PASS |
| 22 | test_e2e_summary_json | e2e | RESOLUTION_SUMMARY.json 与 CSV 一致 | PASS |
| 23 | test_no_first_match_for_ambiguous | forbidden | 禁止 first-match (多个匹配必须标记 ambiguous) | PASS |

### 测试覆盖维度

1. **Unit 测试** (11 项): normalize_filter, match_bias/dark/flat 的所有状态分支
2. **Contract 测试** (4 项): load_calibration_masters 加载 27 master, image_size 统一字段, canonical_filter 归一化, derive_device_id
3. **End-to-End 测试** (3 项): 710 Light 帧端到端解析 + 禁止静默 fallback + summary JSON 一致性
4. **Forbidden Shortcut 测试** (1 项): no first-match for ambiguous
5. **真实数据测试** (在 e2e 中): T2 25 Lum + T4 98 Lum = 123 missing_lum_flat (与 P10-002 一致)
6. **旧功能回归** (在 contract 中): P10-004 FILTER_ALIAS_MAP.json 52 别名全部可归一化

### 禁止捷径验证

| 禁止项 | 测试 | 结果 |
|--------|------|------|
| 不得 first-match | test_no_first_match_for_ambiguous: 2 个相同 Bias 必须返回 ambiguous 而非 unique | PASS |
| 不得静默选择歧义 | test_e2e_no_silent_fallback: 123 unresolved 的 flat_master 必须为空字符串 | PASS |
| 不得用其他滤镜 flat 替代 | test_match_flat_missing_no_master: T2 缺 Lum flat 时返回 missing_lum_flat 而非选 Red/Blue flat | PASS |
| 不得未声明 fallback | test_match_dark_fallback_longest: 全部 dark < Light exp 时显式标记 fallback_longest + reason | PASS |

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python test_resolver.py` | 60s | 0 |
| `python resolve_light_to_master.py` (e2e 数据生成) | 60s | 0 |

### 原始日志

```
======================================================================
P10-005 test_resolver.py 启动
======================================================================
PASS test_normalize_filter_direct_mapping
PASS test_normalize_filter_case_insensitive
PASS test_normalize_filter_unknown
PASS test_match_bias_unique
PASS test_match_bias_missing
PASS test_match_bias_ambiguous
PASS test_match_dark_exact
PASS test_match_dark_closest_longer
PASS test_match_dark_fallback_longest
PASS test_match_dark_missing
PASS test_match_dark_ambiguous_exact
PASS test_match_flat_unique
PASS test_match_flat_missing_no_filter
PASS test_match_flat_missing_no_master
PASS test_match_flat_ambiguous
PASS test_load_masters_count
PASS test_load_masters_image_size_unified
PASS test_load_masters_canonical_filter
PASS test_derive_device_id
PASS test_e2e_resolution_csv
PASS test_e2e_no_silent_fallback
PASS test_e2e_summary_json
PASS test_no_first_match_for_ambiguous

======================================================================
汇总: PASS=23, FAIL=0, TOTAL=23
======================================================================
```

## 结果与证据

- 23/23 测试 PASS (100%)
- 0 失败
- 0 跳过
- 0 错误

### 端到端验证（test_e2e_resolution_csv）

| 指标 | 期望 | 实际 | 结果 |
|------|------|------|------|
| 总 Light 帧 | 710 | 710 | PASS |
| Resolved | 587 | 587 | PASS |
| Unresolved | 123 | 123 | PASS |
| Bias 状态 | 全部 unique | {unique} | PASS |
| Dark 状态 | 全部 exact | {exact} | PASS |
| Flat unique | 587 | 587 | PASS |
| Flat missing | 123 | 123 | PASS |
| 歧义 NONE | 587 | 587 | PASS |
| 歧义 FLAT_MISSING | 123 | 123 | PASS |
| T2 Lum unresolved | 25 (LDN43 10 + NGC247 15) | 25 | PASS |
| T4 Lum unresolved | 98 (Victory_Nebula 49+49) | 98 | PASS |
| 123 unresolved 全部 missing_lum_flat | 是 | 是 | PASS |
| 123 unresolved 设备 | T2/T4 | T2/T4 | PASS |

## 风险/回滚/残留

- **fallback_longest 分支未在真实数据触发**：本次 710 Light 全部有精确 Dark 匹配，但单元测试 test_match_dark_fallback_longest 已覆盖此分支
- **ambiguous 分支未在真实数据触发**：本次 27 Master 无重复，但单元测试 test_match_bias_ambiguous / test_match_dark_ambiguous_exact / test_match_flat_ambiguous 已覆盖此分支
- **image_size 统一字段依赖 filename 兜底**：P10-003 的 image_size_from_header 全空，resolver 用 filename 兜底。建议后续修复 P10-003 的 XISF Image geometry 解析

## 结论

P10-005 测试 PASS。23/23 测试通过，覆盖 unit/contract/e2e/forbidden shortcut 四个维度。端到端验证 710 Light 帧 resolved=587 + unresolved=123 (全部 missing_lum_flat, 与 P10-002 设备档案一致)。禁止捷径检查通过：无 first-match，无静默 fallback，无未声明 fallback。Bias 100% unique, Dark 100% exact, Flat 587 unique + 123 missing。
