# 证据索引

- Task/ADR：P10-001 读取全部TestData子目录说明文档
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3 (astropy) + PowerShell 7

## 目标/问题

递归读取 testdata/ 下全部子目录的说明文档，交叉读取 FITS/XISF Header，输出设备档案、数据集清单、滤镜别名映射和冲突报告。

## 输入与范围

- 输入：testdata/ 下 10 个子目录（7 数据集 + 3 校准目录）
- 依赖：P09-001（已满足）
- 参考规范：docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md

## 执行/决策

1. 递归发现 10 个 TestData 子目录
2. 解析 7 个说明文档（素材信息*.txt）
3. 读取 49 个 FITS Header 采样（每个 target/device/panel/filter 分组 1 个）
4. 读取 27 个 XISF Header（全部校准文件）
5. 交叉验证文档与 Header，检测冲突
6. 输出 4 个交付物

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_testdata_catalog.py` | 120s | 0 |

## 结果与证据

### 证据目录结构

```
engineering_v1.2/evidence/P10-001/
├── TASK_REPORT.md                          # 任务报告
├── TEST_REPORT.md                          # 测试报告 (12/12 PASS)
├── EVIDENCE_INDEX.md                       # 本文件
├── REVIEW_REPORT.md                        # 独立复核报告
├── TESTDATA_EQUIPMENT_CATALOG.csv          # 主交付物: 设备档案 (3 行 T2/T3/T4)
├── TESTDATA_DATASET_CATALOG.csv            # 主交付物: 数据集清单 (49 行)
├── FILTER_ALIAS_MAP.json                   # 主交付物: 滤镜别名映射 (6 规范名)
├── DOCUMENT_FACT_CONFLICTS.md              # 主交付物: 冲突报告 (1 个冲突)
├── scripts/
│   └── extract_testdata_catalog.py         # 提取脚本
└── raw_logs/
    ├── extract_testdata_catalog.log        # 脚本运行日志
    ├── run_extract_final.log               # 最终运行日志
    └── header_samples.json                 # 所有 Header 采样 (FITS + XISF + doc_records)
```

### 关键统计

| 指标 | 值 |
|------|-----|
| TestData 子目录数 | 10 |
| 说明文档数 | 7 |
| 设备数 | 3 (T2/T3/T4, 无 T1) |
| Light 分组数 | 49 |
| FITS Header 采样数 | 49 |
| XISF Header 采样数 | 27 |
| 冲突数 | 1 |
| 硬门限 | PASS |

### 冲突汇总

| 冲突类型 | 规范名 | 别名 | 处理任务 |
|----------|--------|------|----------|
| calibration_filter_alias_inconsistency | OIII | OIII, Oiii | P10-004 |

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（P10-005 须处理）
- OIII 别名不一致（P10-004 统一）
- NGC83_cluster 文件名前缀 NGC90（P10-003 核实）

## 结论

P10-001 完成。4 个交付物已生成，硬门限 PASS。1 个冲突已记录并将在后续任务解决。
