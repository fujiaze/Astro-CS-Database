# 证据索引

- Task/ADR：P10-005 实现并验证 Light 到 Master 唯一解析
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

P10-005 实现 Light 到 Bias/Dark/Flat Master 的唯一解析 resolver，对全 710 Light 帧输出每帧的选择理由与歧义。本索引列出全部证据文件，便于独立复核。

## 输入与范围

- 任务定义：`tasks/P10-005.md`
- 规范：`docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`
- 依赖（已满足）：P10-002 + P10-003 + P10-004
- 数据源：P10-001 header_samples.json + P10-002 device profiles + P10-003 master inventory + P10-004 filter alias map + testdata 710 Light 帧

## 执行/决策

证据归档结构：

```
engineering_v1.2/evidence/P10-005/
├── TASK_REPORT.md                          # 任务报告
├── TEST_REPORT.md                          # 测试报告 (23/23 PASS)
├── EVIDENCE_INDEX.md                       # 本文件
├── REVIEW_REPORT.md                        # 复核报告
├── LIGHT_TO_MASTER_RESOLUTION.csv          # 710 行 Light -> Master 解析结果
├── UNRESOLVED_CALIBRATION_REPORT.md        # 123 unresolved 详情 (全部 missing_lum_flat)
├── RESOLUTION_SUMMARY.json                 # 汇总统计
├── scripts/
│   ├── resolve_light_to_master.py          # resolver 主脚本
│   ├── test_resolver.py                   # 23 项单元/契约/e2e 测试
│   └── diagnose_match.py                   # 0 resolved 根因诊断脚本
└── raw_logs/
    ├── resolve_light_to_master.log         # resolver 运行日志
    └── test_resolver.log                  # 测试运行日志
```

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 | 日志文件 |
|------|------|--------|---------|
| `python resolve_light_to_master.py` | 60s | 0 | `raw_logs/resolve_light_to_master.log` |
| `python test_resolver.py` | 60s | 0 | `raw_logs/test_resolver.log` |
| `python diagnose_match.py` (诊断, 非交付) | 30s | 0 | (stdout only) |

## 结果与证据

### 交付物

| # | 文件 | 说明 | 行数/大小 |
|---|------|------|----------|
| 1 | `LIGHT_TO_MASTER_RESOLUTION.csv` | 710 Light 帧 Bias/Dark/Flat 解析结果 | 710 行 + header |
| 2 | `UNRESOLVED_CALIBRATION_REPORT.md` | 123 unresolved 详情 (全部 missing_lum_flat) | 143 行 |
| 3 | `RESOLUTION_SUMMARY.json` | 汇总统计 JSON | 36 行 |
| 4 | `scripts/resolve_light_to_master.py` | resolver 主脚本 | 460 行 |
| 5 | `scripts/test_resolver.py` | 23 项测试脚本 | 499 行 |

### 关键统计

| 指标 | 值 |
|------|-----|
| 总 Light 帧 | 710 |
| Resolved | 587 (82.7%) |
| Unresolved | 123 (17.3%) |
| Bias 状态 | 710 unique |
| Dark 状态 | 710 exact |
| Flat 状态 | 587 unique + 123 missing |
| 歧义分类 | 587 NONE + 123 FLAT_MISSING |
| 测试通过率 | 23/23 (100%) |
| T2 Lum unresolved | 25 (与 P10-002 一致) |
| T4 Lum unresolved | 98 (与 P10-002 一致) |
| T3 Lum resolved | 22 (T3 有 Lum flat) |

### 关键发现

1. **0 resolved 根因诊断**：P10-003 的 XISF header 解析器未提取 NAXIS1/NAXIS2（存于 Image 元素属性，非 FITSKeyword），导致 `image_size_from_header` 全空。修复：resolver 使用统一 `image_size` 字段（header 优先，filename 兜底）。
2. **123 unresolved 全部为 missing_lum_flat**：与 P10-002 设备档案 `missing_flats=[Lum]` 一致（T2/T4），resolver 正确标记，未静默替代。
3. **Bias/Dark 100% 匹配**：27 个 Master 覆盖全部 T2/T3/T4 的 Bias 和 Dark 需求。
4. **禁止捷径 PASS**：所有 missing 的 Light 帧 `flat_master` 字段为空字符串，未选其他滤镜 flat 作替代。
5. **Dark 兜底未触发**：本次全部有精确 Dark 匹配，无 fallback_longest 场景。单元测试已覆盖。

## 风险/回滚/残留

- **123 Lum Light 帧 BLOCKED**：T2/T4 缺 Lum Flat，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录，本任务正确标记未静默替代。
- **image_size_from_header 全空**：P10-003 的 XISF header 解析器未提取 NAXIS1/NAXIS2，本任务用 filename 兜底。建议后续修复 P10-003 的 XISF Image geometry 解析。
- **fallback_longest / ambiguous 未在真实数据触发**：单元测试已覆盖，但真实数据无此场景。

## 结论

P10-005 证据完整。3 个数据交付物（CSV + MD + JSON）+ 2 个脚本交付物（resolver + tests）+ 2 个原始日志全部归档。587/710 Light 帧 resolved，123/710 unresolved（全部 T2/T4 Lum Light 缺 Flat，与 P10-002 设备档案一致，未静默替代）。23/23 测试 PASS。禁止捷径检查通过。0 resolved 根因已诊断并修复。
