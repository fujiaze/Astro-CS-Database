# P12-003 任务报告：验证光谱积分与响应曲线无回归

- **任务编号**: P12-003
- **Gate**: G12 (Photometric Diagnostic)
- **开始时间**: 2026-07-28
- **完成时间**: 2026-07-28
- **状态**: DONE
- **依赖**: P12-001 (DONE)
- **参考**: `docs/06_PHOTOMETRY_CORRECTION_SPEC.md`
- **后续**: P12-004 (T1-T4 与滤镜类别测光矩阵验证)

## 1. 任务目标

P12-002 修复了 KD-tree 方向 bug 并实现双向最近邻唯一配对。P12-003 任务为回归验证任务，目标：

1. 验证 P12-002 的匹配修复未引入光谱积分与响应曲线回归
2. 用固定光谱/滤镜/QE 黄金样本验证 F_syn 一致性和单位
3. 提供 filter/QE provenance 溯源报告

## 2. 完成内容

### 2.1 滤光片和 QE 曲线溯源（test1_provenance）

完成全部滤光片和 CCD QE 曲线的数据溯源：

- **滤光片**: 38 种（涵盖 Antlia V Pro Series、Astrodon E/I-series、Astronomik Deep Sky/Typ 2c/UV-IR Block、Baader、Chroma、IDAS LPS P3、Johnson U/B/V/R/I、Optolong、OPTOLONG L-PRO、SDSS u/g/r/i/z、ZWO）
- **QE 曲线**: 13 种（GSENSE2020BSI、GSENSE400BSI/FSI、GSENSE4040BSI/FSI、Ideal QE curve、KAF-16200/16803/8300、Sony IMX183/IMX411 系列系列/IMX492）
- **波长范围**: 全部滤光片和 QE 曲线波长范围正常（滤光片覆盖 300-1100nm，QE 曲线覆盖 200-1100nm）
- **值域范围**: 全部透过率/QE 值在 [0, 1] 区间内，峰值 0.86-1.0 合理

### 2.2 C++ 与 Python F_syn 一致性验证（test2_fsyn_consistency）

使用合成黑体光谱作为黄金样本，对比 C++ 实现 (`compute_f_syn` / `compute_f_syn_cached`) 与 Python 实现 (`SyntheticPhotometry.compute`) 的一致性：

- **温度范围**: 3500K, 4500K, 5800K, 7500K, 10000K（5 个温度）
- **星等范围**: mag_g = 10, 12, 15（3 个星等）
- **滤光片**: Antlia V Pro Series B, ZWO R（2 个代表滤光片，覆盖蓝端和红端）
- **QE 状态**: 无 QE + GSENSE2020BSI QE（2 种状态）
- **组合总数**: 60 组对比（5 × 3 × 2 × 2）
- **容忍标准**: 相对误差 < 1.0%

**结果**:
- 非缓存版本 (`compute_f_syn` vs Python): 最大相对误差 ≈ 1.06e-6（远优于 1% 标准）
- 缓存版本 (`compute_f_syn_cached` vs Python): 最大相对误差 ≈ 2.78e-4（即 0.028%，远优于 1% 标准）

### 2.3 缓存版本一致性验证（test3_cached_vs_uncached）

对比 C++ 缓存版本 (`compute_f_syn_cached`) 与非缓存版本 (`compute_f_syn`) 的一致性：

- **组合总数**: 60 组对比
- **容忍标准**: 相对误差 < 0.03%
- **结果**: 最大相对误差 ≈ 2.78e-4（即 0.028%），全部 PASS

缓存版本与非缓存版本的微小差异源自网格插值精度差异（缓存版本预计算响应曲线网格，非缓存版本逐点计算），在容忍范围内。

### 2.4 无 QE 与 QE=1.0 等价性验证（test4_no_qe_vs_qe1）

验证不传入 QE 参数与传入 QE=1.0（理想 QE 曲线）的结果完全等价：

- **测试条件**: T=5800K, mag_g=12, Antlia V Pro Series B 滤光片
- **结果**: 相对误差 = 0.0（完全一致）
- **spec_count**: 343（光谱采样点数一致）

### 2.5 现有测光校准测试无回归（test5_regression）

运行现有测光校准测试套件，验证 P12-002 修复未引入回归：

- **测试框架**: pytest 9.1.1 + Python 3.10.11
- **结果**: 5/5 PASS
- **测试详情**:
  - 测试1 基本测光校准 (10星): n_matched=10, scale=10.0, fit_used=10 ✓
  - 测试2 MAD离群清洗 (20星): n_matched=19, scale=9.997, fit_used=19 ✓
  - 测试3 无Gaia星退化路径: n_matched=0, scale=1.0 ✓
  - 测试4 SIP WCS投影 (10星): n_matched=10, scale=10.0 ✓
  - 测试5 P12-001 diag 输出: 全部 20 字段正确填充 ✓

## 3. 测试文件

### 3.1 C++ 测试

- **文件**: `lib/photometric_calib/cpp/test/test_spectrum_integrator.cpp`
- **编译产物**: `lib/photometric_calib/cpp/test/test_spectrum_integrator.exe`
- **覆盖**: test1 溯源、test2 F_syn 一致性、test3 缓存一致性、test4 QE 等价性

### 3.2 Python Golden 测试

- **文件**: `lib/photometric_calib/cpp/test/test_spectrum_integrator_golden.py`
- **覆盖**: 调用 C++ DLL 导出函数，与 Python `SyntheticPhotometry.compute` 对比，生成 test_results.json
- **回归测试**: 调用现有 `test_photometric_calib.py` 套件验证无回归

## 4. 关键指标汇总

| 指标 | 实际值 | 容忍标准 | 状态 |
|------|--------|----------|------|
| test1 滤光片数 | 38 | - | PASS |
| test1 QE 曲线数 | 13 | - | PASS |
| test2 F_syn 对比组数 | 60 | - | PASS |
| test2 最大 uncached 相对误差 | 1.06e-6 | < 1.0% | PASS |
| test2 最大 cached 相对误差 | 2.78e-4 (0.028%) | < 1.0% | PASS |
| test3 cached vs uncached 对比组数 | 60 | - | PASS |
| test3 最大相对误差 | 2.78e-4 (0.028%) | < 0.03% | PASS |
| test4 无 QE vs QE=1.0 相对误差 | 0.0 | = 0 | PASS |
| test5 回归测试通过率 | 5/5 | 5/5 | PASS |

## 5. 结论

P12-002 修复（KD-tree 方向 bug 修复 + 双向最近邻唯一配对）**未引入光谱积分与响应曲线回归**：

1. ✅ 滤光片（38种）和 QE 曲线（13种）溯源完成，数据完整
2. ✅ C++ 与 Python F_syn 一致性验证通过（60 组对比，最大相对误差远优于 1%）
3. ✅ 缓存版本与非缓存版本一致性验证通过（60 组对比，相对误差 < 0.03%）
4. ✅ 无 QE 与 QE=1.0 等价性验证通过（相对误差 = 0）
5. ✅ 现有测光校准测试无回归（5/5 PASS）

## 6. 约束遵守

- ✅ 不修改测试代码本身（仅运行和验证）
- ✅ 不改 IRLS/Tukey 稳健拟合逻辑
- ✅ 不改 C API 接口
- ✅ 不改 PhotometricDiag 结构体
- ✅ 使用 PowerShell 7 环境
- ✅ 使用中文回复
- ✅ 不把匹配修复误改积分结果

## 7. 关键证据文件

- 测试结果 JSON: `reports/test_results.json`
- 滤光片/QE 溯源: `reports/filter_qe_provenance.json`
- 原始日志: `raw_logs/test_spectrum_integrator_golden.log`
- 任务报告: `TASK_REPORT.md` (本文件)
- 测试报告: `TEST_REPORT.md`
- 证据索引: `EVIDENCE_INDEX.md`
- 复核报告: `REVIEW_REPORT.md`
