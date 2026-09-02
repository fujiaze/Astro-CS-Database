# P11-002 — 证据索引

## 证据清单

## 1. 源代码

| 文件 | 描述 | 路径 |
|------|------|------|
| wcs_closure_diagnostic.py | 核心诊断模块 (独立 astropy WCS) | `scripts/wcs_closure_diagnostic.py` |
| test_wcs_closure.py | 单元测试 (30 项, 含 5 项独立性硬约束) | `scripts/test_wcs_closure.py` |
| run_diagnostic.py | driver 脚本 (复制→求解→诊断→汇总) | `scripts/run_diagnostic.py` |
| check_fits_header.py | FITS header 检查工具 | `scripts/check_fits_header.py` |

## 2. 测试证据

| 文件 | 描述 | 路径 |
|------|------|------|
| unit_test.log | 单元测试运行日志 (30/30 PASS) | `raw_logs/unit_test.log` |
| run_diagnostic.log | 完整 driver 运行日志 | `raw_logs/run_diagnostic.log` |

## 3. 诊断报告

### T3_LUM_NGC55 (T3 / LUM / NGC55 / 4096×4096)

| 文件 | 描述 | 关键指标 |
|------|------|----------|
| closure_report.json | 闭环报告 | median=0.897 px, p90=1.284 px, p99=1.778 px, gate=FAIL |
| matched_pairs.json | 匹配星对数据 | n=702 |
| residual_plot.png | 残差分布图 (4 子图) | X median=0.218, Y median=0.848 |
| quadrant_plot.png | 四象限分布图 | Q1=350, Q2=345, Q3=369, Q4=419 |

### T2_HA_LDN43 (T2 / HA / LDN43 / 4096×4096)

| 文件 | 描述 | 关键指标 |
|------|------|----------|
| closure_report.json | 闭环报告 | median=0.772 px, p90=0.958 px, p99=2.058 px, gate=FAIL |
| matched_pairs.json | 匹配星对数据 | n=1237 |
| residual_plot.png | 残差分布图 (4 子图) | X median=0.498, Y median=0.555 |
| quadrant_plot.png | 四象限分布图 | Q1=308, Q2=321, Q3=315, Q4=403 |

### 汇总

| 文件 | 描述 | 路径 |
|------|------|------|
| driver_summary.json | 两帧汇总报告 | `reports/driver_summary.json` |

## 4. 工作产物

| 文件 | 描述 | 路径 |
|------|------|------|
| T3_LUM_NGC55_solved.fits | T3 LUM 已求解 FITS (含 WCS) | `work/T3_LUM_NGC55_solved.fits` |
| T2_HA_LDN43_solved.fits | T2 HA 已求解 FITS (含 WCS) | `work/T2_HA_LDN43_solved.fits` |

## 5. 文档

| 文件 | 描述 | 路径 |
|------|------|------|
| TASK_REPORT.md | 任务报告 | `TASK_REPORT.md` |
| TEST_REPORT.md | 测试报告 | `TEST_REPORT.md` |
| EVIDENCE_INDEX.md | 证据索引 (本文件) | `EVIDENCE_INDEX.md` |
| REVIEW_REPORT.md | 复核报告 | `REVIEW_REPORT.md` |

## 6. 规范引用

| 文件 | 描述 | 路径 |
|------|------|------|
| 05_WCS_COORDINATE_CONVENTION_AND_CLOSURE_SPEC.md | WCS 坐标约定与闭环规范 | `engineering_v1.2/docs/` |
| COORDINATE_CONVENTION.md | 坐标约定文档 (P11-001 输出) | `engineering_v1.2/evidence/P11-001/` |

## 证据完整性核查

- [x] 单元测试日志完整 (30/30 PASS)
- [x] Driver 运行日志完整 (2/2 帧成功)
- [x] 两帧闭环报告 JSON 完整
- [x] 两帧匹配星对 JSON 完整
- [x] 两帧残差图 PNG 完整
- [x] 两帧四象限图 PNG 完整
- [x] Driver 汇总 JSON 完整
- [x] 已求解 FITS 文件完整 (2 帧)
- [x] 任务报告完整
- [x] 测试报告完整
- [x] 证据索引完整
- [x] 复核报告完整
