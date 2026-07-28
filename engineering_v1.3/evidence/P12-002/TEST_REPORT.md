# P12-002 测试报告

- **任务编号**: P12-002
- **测试日期**: 2026-07-28
- **测试环境**: Windows + PowerShell 7 + Python 3.10.11 + pytest 9.1.1
- **被测 DLL**: `lib/photometric_calib/cpp/photometric_calib.dll` (1065.4 KB, g++ 16.1.0)
- **测试文件**: `lib/photometric_calib/cpp/test/test_photometric_calib.py`

## 1. 测试总览

**结果: 5/5 PASS**（修复前 2/5 PASS, 3/5 FAIL）

```
============================== test session starts =============================
platform win32 -- Python 3.10.11, pytest-9.1.1, pluggy-1.6.0
collected 5 items

test/test_photometric_calib.py::test_basic_calibration PASSED
test/test_photometric_calib.py::test_outlier_cleaning PASSED
test/test_photometric_calib.py::test_no_gaia PASSED
test/test_photometric_calib.py::test_sip_wcs PASSED
test/test_photometric_calib.py::test_diag_output PASSED

======================== 5 passed, 5 warnings in 0.64s ========================
```

## 2. 测试详情

### 2.1 测试1: 基本测光校准 (10 颗星, TAN 投影)

**修复前**: FAIL (n_matched=1, KD-tree bug 导致仅匹配 1 颗)
**修复后**: PASS

```
[测试1] 基本测光校准 (10颗星, TAN投影)
  n_matched = 10 (期望 10)
  scale = 1.000000e+01 (期望 ~10.0)
  out_img[0,0] = 10000.0000 (期望 ~10000.0)
  out_img shape = (200, 200)
  diag.fit_used = 10 (期望 10)
  diag.psf_valid = 10 (期望 10)
  [PASS] 基本测光校准
```

**关键日志**:
```
[INFO] [star_matcher] P12-002 正向匹配 (PSF→Gaia): 10 / 10 命中
[INFO] [star_matcher] P12-002 反向匹配 (Gaia→PSF): 10 / 10 命中
[INFO] [star_matcher] P12-002 阶段4/5/6: spatial_candidates=10 (正向命中),
       unique_matches=10 (双向唯一), rejected_ambiguous=0 (非互为最近邻),
       rejected_distance=0 (距离超阈值)
[INFO] [star_matcher] P12-001 阶段8: match_distance median=0.1414 p90=0.1414 max=0.1414 px
[INFO] [star_matcher] P12-002 KD-tree 双向匹配完成: 10 对 (正向 10, 歧义 0)
```

### 2.2 测试2: MAD 离群清洗 (20 颗星, 注入 1 颗离群)

**修复前**: FAIL (n_matched<19, KD-tree bug 导致匹配数不足)
**修复后**: PASS

```
[测试2] MAD离群清洗 (注入1颗离群星)
  n_matched = 19 (期望 19, 离群1被剔除)
  scale = 9.997411e+00 (期望 ~10.0)
  diag.fit_used = 19 (期望 19)
  diag.rejected_quality = 1
  [PASS] MAD离群清洗
```

**关键日志**:
```
[INFO] [star_matcher] P12-002 正向匹配 (PSF→Gaia): 20 / 20 命中
[INFO] [star_matcher] P12-002 反向匹配 (Gaia→PSF): 20 / 20 命中
[INFO] [star_matcher] P12-002 阶段4/5/6: spatial_candidates=20 (正向命中),
       unique_matches=20 (双向唯一), rejected_ambiguous=0 (非互为最近邻),
       rejected_distance=0 (距离超阈值)
[INFO] [star_matcher] 星等一致性: 通过 19, 拒绝 1 (median_delta=-21.2474, tol=3.00 mag)
[INFO] [star_matcher] IRLS 收敛于迭代 3: location=-0.999888
[INFO] [star_matcher] P12-001 阶段6/7/8: rejected_quality=1 (invalid=0 + mag=1 + irls=0),
       fit_used=19, robust_iterations=4, scale=9.997411e+00, sigma=0.003365
```

### 2.3 测试3: 无 Gaia 星退化路径

**修复前**: PASS (不依赖 KD-tree)
**修复后**: PASS (保持不变)

```
[测试3] 无Gaia星退化路径 (scale=1.0)
  n_matched = 0 (期望 0)
  scale = 1.000000e+00 (期望 1.0)
  out_img[0,0] = 500.0000 (期望 500.0)
  diag.spectrum_rows_total = 0 (期望 0)
  [PASS] 退化路径
```

### 2.4 测试4: SIP WCS 投影 (二阶 SIP, 10 颗星)

**修复前**: FAIL (n_matched<8, KD-tree bug)
**修复后**: PASS

```
[测试4] SIP WCS投影 (二阶SIP)
  n_matched = 10 (期望 10, SIP投影后仍能匹配)
  scale = 1.000000e+01
  diag.fit_used = 10
  [PASS] SIP WCS投影
```

**关键日志**:
```
[INFO] [star_matcher] P12-002 正向匹配 (PSF→Gaia): 10 / 10 命中
[INFO] [star_matcher] P12-002 反向匹配 (Gaia→PSF): 10 / 10 命中
[INFO] [star_matcher] P12-002 KD-tree 双向匹配完成: 10 对 (正向 10, 歧义 0)
```

### 2.5 测试5: P12-001 PhotometricDiag 分阶段诊断输出

**修复前**: PASS (diag 字段填充正常，但值反映 KD-tree bug)
**修复后**: PASS (diag 字段值正确反映双向匹配)

```
[测试5] P12-001 PhotometricDiag 分阶段诊断输出
  diag.to_dict() = {
    'spectrum_rows_total': 0, 'valid_fsyn': 0,
    'gaia_projected_in_frame': 10,
    'psf_total': 10, 'psf_valid': 10,
    'spatial_candidates': 10, 'unique_matches': 10,
    'rejected_ambiguous': 0, 'rejected_distance': 0, 'rejected_quality': 0,
    'fit_used': 10, 'robust_iterations': 0,
    'scale_factor': 10.0, 'sigma_residual': 0.0,
    'r_median': -1.0, 'r_p90': -1.0, 'r_max': -1.0,
    'match_distance_median': 0.14142135623789936,
    'match_distance_p90': 0.1414213562382129,
    'match_distance_max': 0.1414213562388188
  }
  [PASS] psf_total > 0
  [PASS] psf_valid > 0 (实际=10)
  [PASS] fit_used > 0 (实际=10)
  [PASS] scale_factor > 0 (实际=1.000000e+01)
  [PASS] diag.scale_factor ≈ scale (1.000000e+01 vs 1.000000e+01)
  [PASS] diag.sigma_residual ≈ sigma_residual (0.000000 vs 0.000000)
  [PASS] P12-001 diag 输出
```

## 3. diag 各字段值汇总

### 测试1 (10 颗完全一致星, F_instr=F_syn/10)

| 字段 | 值 | 说明 |
|------|-----|------|
| spectrum_rows_total | 0 | pc_calibrate_simple 不计算 F_syn |
| valid_fsyn | 0 | 同上 |
| gaia_projected_in_frame | 10 | 全部投影成功 |
| psf_total | 10 | 输入 10 颗 (测试1实际11颗含1失败, psf_valid=10) |
| psf_valid | 10 | status==0 的 PSF 星 |
| spatial_candidates | 10 | 正向 KD-tree 命中 |
| unique_matches | 10 | 双向唯一匹配 |
| rejected_ambiguous | 0 | 无歧义对 |
| rejected_distance | 0 | 全部命中 |
| rejected_quality | 0 | 无质量问题 |
| fit_used | 10 | IRLS inliers |
| robust_iterations | 0 | S=0 跳过迭代 (所有 r 相同) |
| scale_factor | 10.0 | 10^(-location) = 10^1 |
| sigma_residual | 0.0 | 所有 r 相同, MAD=0 |
| r_median | -1.0 | log10(0.1) |
| r_p90 | -1.0 | 同上 |
| r_max | -1.0 | 同上 |
| match_distance_median | 0.1414 | sqrt(0.1²+0.1²) |
| match_distance_p90 | 0.1414 | 同上 |
| match_distance_max | 0.1414 | 同上 |

### 测试2 (20 颗星, 注入 1 颗离群)

| 字段 | 值 | 说明 |
|------|-----|------|
| spatial_candidates | 20 | 正向全部命中 |
| unique_matches | 20 | 双向全部唯一 |
| rejected_ambiguous | 0 | 无歧义 |
| rejected_distance | 0 | 全部命中 |
| rejected_quality | 1 | 1 颗星等不一致被剔除 |
| fit_used | 19 | IRLS inliers |
| robust_iterations | 4 | IRLS 收敛 |
| scale_factor | 9.997411e+00 | 接近 10.0 |
| sigma_residual | 0.003365 | 非零, 有微小扰动 |

## 4. 测试结论

- **5/5 PASS**: 所有测试通过，包括之前因 KD-tree bug 失败的 3 个测试
- **KD-tree bug 修复验证**: 修复前 n_matched=1, 修复后 n_matched=10/19/10，匹配数恢复正常
- **双向匹配验证**: 正向/反向命中数一致，unique_matches 等于正向命中数（测试场景无歧义对）
- **质量筛选不变**: 测试2 离群星被正确剔除 (rejected_quality=1, fit_used=19)
- **SIP WCS 不变**: 测试4 SIP 投影后匹配正常
- **退化路径不变**: 测试3 无 Gaia 星时 scale=1.0 退化正常
- **诊断字段完整**: 测试5 所有 20 个 diag 字段正确填充
- **耗时**: 5 个测试 0.64s (含 DLL 加载)
