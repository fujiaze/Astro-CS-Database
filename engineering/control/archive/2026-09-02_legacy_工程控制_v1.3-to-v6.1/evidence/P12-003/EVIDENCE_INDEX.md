# P12-003 证据索引

- **任务编号**: P12-003
- **任务名称**: 验证光谱积分与响应曲线无回归
- **完成日期**: 2026-07-28
- **状态**: DONE
- **Gate**: G12 (Photometric Diagnostic)

## 1. 证据清单

| 类别 | 文件 | 说明 | 大小 | SHA256 |
|------|------|------|------|--------|
| 任务报告 | `TASK_REPORT.md` | 完整任务报告（目标/完成内容/测试文件/关键指标/结论/约束） | - | - |
| 测试报告 | `TEST_REPORT.md` | 5/5 测试详情与关键指标 | - | - |
| 复核报告 | `REVIEW_REPORT.md` | 独立复核报告 (VERDICT: PASS) | - | - |
| 测试结果 JSON | `reports/test_results.json` | 全部 5 个测试的完整结果数据 | 56116 bytes | `997DAC0136283DBF8B14AE5ADA5DBD773A98AC25A633CF50FC3D0495E963C032` |
| 滤光片/QE 溯源 | `reports/filter_qe_provenance.json` | 38 种滤光片 + 13 种 QE 曲线的采样点/波长/值域溯源 | 8460 bytes | `7F35B23DEB25E5A00A35B04EB9FF978CC4A2605EDACAB5708DE878247202E4E6` |
| 原始日志 | `raw_logs/test_spectrum_integrator_golden.log` | 测试运行日志（摘要行） | 62 bytes | `19BB8EDABEF23A7EE1A871E2C57DE5D2FECDE9E9C044B977D4F3525486687D10` |

## 2. 测试代码清单

| 类别 | 文件 | 说明 | 大小 | SHA256 |
|------|------|------|------|--------|
| C++ 测试 | `lib/photometric_calib/cpp/test/test_spectrum_integrator.cpp` | C++ 测试主程序（test1-test4） | 9366 bytes | `C80F6B068DD2D265F5827656E046CB3F306D2D99B9288369E1983CF50156A7CB` |
| Python golden | `lib/photometric_calib/cpp/test/test_spectrum_integrator_golden.py` | Python golden 测试（调用 DLL + 回归测试） | 27800 bytes | `797847543DF656622D7700435769A7BEF2F7B9162576D0F61159FE5F209FFDA1` |

## 3. 新增文件清单（本次任务生成）

### 3.1 证据文件（4 个）

- `工程控制/evidence/P12-003/TASK_REPORT.md`
- `工程控制/evidence/P12-003/TEST_REPORT.md`
- `工程控制/evidence/P12-003/EVIDENCE_INDEX.md` (本文件)
- `工程控制/evidence/P12-003/REVIEW_REPORT.md`

### 3.2 测试产物（已存在，本次任务引用）

- `工程控制/evidence/P12-003/reports/test_results.json`
- `工程控制/evidence/P12-003/reports/filter_qe_provenance.json`
- `工程控制/evidence/P12-003/raw_logs/test_spectrum_integrator_golden.log`

## 4. 关键验证结果

### 4.1 测试结果总览

| 测试 | 结果 | 关键指标 |
|------|------|----------|
| test1_provenance | PASS | 38 滤光片 + 13 QE 曲线溯源完整 |
| test2_fsyn_consistency | PASS (60/60) | 最大 uncached rel_err=1.06e-6, 最大 cached rel_err=2.78e-4 |
| test3_cached_vs_uncached | PASS (60/60) | 最大 rel_err=2.78e-4 (0.028%) |
| test4_no_qe_vs_qe1 | PASS | rel_err=0.0 (完全等价) |
| test5_regression | PASS (5/5) | 现有测光校准测试无回归 |

### 4.2 数值精度

| 指标 | 实际值 | 容忍标准 |
|------|--------|----------|
| test2 最大 uncached 相对误差 | 1.06e-6 | < 1.0% |
| test2 最大 cached 相对误差 | 2.78e-4 (0.028%) | < 1.0% |
| test3 最大相对误差 | 2.78e-4 (0.028%) | < 0.03% |
| test4 相对误差 | 0.0 | = 0 |

## 5. 依赖与后续

- **依赖**: P12-001 (DONE) — PhotometricDiag 结构体与 8 阶段埋点
- **前置**: P12-002 (DONE) — KD-tree 方向 bug 修复 + 双向最近邻唯一配对（本次验证对象）
- **后续**: P12-004 (T1-T4 与滤镜类别测光矩阵验证)
- **Gate**: G12 进行中 (P12-001/002/003 DONE, P12-004~006 TODO)
