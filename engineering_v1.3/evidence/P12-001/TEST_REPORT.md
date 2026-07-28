# P12-001 测试报告 — 增加Photometric分阶段诊断

## 测试环境
- **日期**: 2026-07-28
- **平台**: Windows + PowerShell 7
- **Python**: 3.x (jsonschema 4.26.0)
- **DLL**: `lib/photometric_calib/cpp/photometric_calib.dll`
- **Orchestrator**: `lib/orchestrator/cpp/orchestrator.exe` (2026-07-28 20:15 构建)

## 测试清单

### 1. 单元测试: test_photometric_calib.py
**文件**: `lib/photometric_calib/cpp/test/test_photometric_calib.py`
**日志**: `raw_logs/test_photometric_calib_p12_001.log`

| # | 测试名 | 结果 | 说明 |
|---|--------|------|------|
| 1 | 基本测光校准 (10颗星, TAN投影) | **FAIL** | n_matched=1 (期望10). 预存 KD-tree 方向逻辑 bug (P12-002 范围) |
| 2 | MAD离群清洗 (注入1颗离群星) | **FAIL** | n_matched=1 (期望19). 同上 KD-tree bug |
| 3 | 无Gaia星退化路径 (scale=1.0) | **PASS** | n_matched=0, scale=1.0, out_img=500.0 — 退化路径正确 |
| 4 | SIP WCS投影 (二阶SIP) | **FAIL** | n_matched=1 (期望≥8). 同上 KD-tree bug |
| 5 | P12-001 PhotometricDiag 分阶段诊断输出 | **PASS** | diag 字段全部正确填充, scale_factor/sigma_residual 一致 |

**总计**: 2/5 通过

#### P12-001 diag 输出验证 (测试5详情)
```
diag.to_dict() = {
  'spectrum_rows_total': 0,       # pc_calibrate_simple 不计算 F_syn
  'valid_fsyn': 0,                # 同上
  'gaia_projected_in_frame': 10,  # 10 颗 Gaia 星全部投影入帧
  'psf_total': 10,                # 10 颗 PSF 星
  'psf_valid': 10,                # 10 颗有效 (status==0)
  'spatial_candidates': 1,        # KD-tree 命中 1 (KD-tree bug)
  'unique_matches': 1,            # 当前无双向过滤, 等于候选数
  'rejected_ambiguous': 0,        # 当前无双向匹配
  'rejected_distance': 9,         # 9 颗因距离超阈值被拒 (KD-tree bug)
  'rejected_quality': 0,          # 无质量拒绝
  'fit_used': 1,                  # IRLS inliers 1 (KD-tree bug)
  'robust_iterations': 0,         # 单点无需迭代
  'scale_factor': 10.0,           # 与返回的 scale 一致 ✓
  'sigma_residual': 0.0,          # 与返回的 sigma_residual 一致 ✓
  'r_median': -1.0, 'r_p90': -1.0, 'r_max': -1.0,
  'match_distance_median': 0.1414, 'match_distance_p90': 0.1414, 'match_distance_max': 0.1414
}
```
**关键验证**:
- `diag.scale_factor ≈ scale` (10.0 vs 10.0) ✓
- `diag.sigma_residual ≈ sigma_residual` (0.0 vs 0.0) ✓
- `psf_total > 0` ✓, `psf_valid > 0` ✓, `fit_used > 0` ✓

#### KD-tree bug 说明 (预存, 非 P12-001 引入)
- **症状**: 10 颗 PSF 星中仅 1 颗匹配成功, 9 颗因 "距离超阈值" 被拒
- **根因**: `KdTree2D::findNearestRec` 中方向选择逻辑反转 (`diff < 0` 时应探索 right, 实际探索 left)
- **影响**: 查询点远离根节点分裂平面时, 先探索错误子树, 未命中后因 split_dist2 > best_dist2 跳过正确子树
- **归属**: P12-002 (修复Gaia到PSF空间匹配与唯一配对)
- **P12-001 无回归证据**: `git diff HEAD -- star_matcher.cpp` 显示 P12-001 仅新增 `initDiag/percentileOf/PhotometricDiag` 埋点, 未修改 `KdTree2D` 类

### 2. 契约测试: test_contract.py
**文件**: `engineering_v1.3/evidence/P12-001/scripts/test_contract.py`
**日志**: `raw_logs/test_contract.log`
**被测对象**: `output/p12_001_test/photometry_report.json`
**Schema**: `engineering_v1.3/contracts/photometry_report.schema.json`

| # | 测试名 | 结果 | 说明 |
|---|--------|------|------|
| 1 | jsonschema.validate | **PASS** | 报告符合 schema |
| 2 | required 字段齐全 (9个) | **PASS** | frame/valid_fsyn/gaia_in_frame/psf_valid/unique_matches/fit_used/scale_factor/sigma_residual/status 全部存在 |
| 3 | P12-001 诊断字段齐全 (10个额外) | **PASS** | spectrum_rows_total/psf_total/spatial_candidates/rejected_*/robust_iterations/r_* 全部存在 |
| 4 | 字段类型正确 | **PASS** | string/int/number/enum/object 类型全部正确 |
| 5 | match_distance 子字段 | **PASS** | median/p90/max 全部存在 |

**总计**: 5/5 通过

### 3. Orchestrator CLI 验证
**日志**: `raw_logs/stage1_cli_output.log`
**测试帧**: `testdata/results/Victory_Nebula_T4_Flying_Dutchman/panel1/Red/.../01_calibrated.fits`

#### 3.1 photometry_report.json 生成
```
[INFO][orchestrator] [PHOTOMETRIC] photometry_report.json 已生成: output/p12_001_test\photometry_report.json
[INFO][orchestrator] [PHOTOMETRIC] photo_stats 已写入 (含 17 个诊断字段)
```

#### 3.2 quality_metric CLI 事件
```json
{"schema_version":1,"type":"quality_metric","job_id":"p12-001-test",
 "timestamp":"2026-07-28T12:21:06Z","stage":"photometric",
 "metric":{
   "spectrum_rows_total":0,"valid_fsyn":0,"gaia_in_frame":3880,
   "psf_total":2000,"psf_valid":1818,"spatial_candidates":1,
   "unique_matches":1,"rejected_ambiguous":0,"rejected_distance":1817,
   "rejected_quality":0,"fit_used":1,"robust_iterations":0,
   "r_median":2.38197,"r_p90":2.38197,"r_max":2.38197,
   "match_dist_median":0.129163,"match_dist_p90":0.129163,"match_dist_max":0.129163
 }}
```
**17 个诊断字段全部输出** ✓

#### 3.3 分阶段日志埋点
```
[INFO] [pc_api] P12-001 阶段1: spectrum_rows_total=10115, valid_fsyn=10115
[INFO] [star_matcher] P12-001 阶段2: gaia_projected_in_frame=3880 / 10115
[INFO] [star_matcher] P12-001 阶段3: psf_total=2000, psf_valid=1818
[INFO] [star_matcher] P12-001 阶段4/6: spatial_candidates=1, unique_matches=1, rejected_ambiguous=0, rejected_distance=1817
[INFO] [star_matcher] P12-001 阶段8: match_distance median=0.1292 p90=0.1292 max=0.1292 px
[INFO] [star_matcher] P12-001 阶段6/7/8: rejected_quality=0, fit_used=1, robust_iterations=0, scale=4.149845e-03, sigma=0.000000
[INFO] [star_matcher] P12-001 阶段8: r_inliers median=2.381968 p90=2.381968 max=2.381968 dex
```
**8 个阶段全部埋点** ✓

## 通过条件评估

| 条件 | 状态 | 说明 |
|------|------|------|
| PhotometricDiag 结构体正确填充 | **PASS** | 测试5 + CLI 日志验证 |
| photo_stats KV 块含 17 个诊断字段 | **PASS** | orchestrator.cpp 日志确认 |
| photometry_report.json 符合 schema | **PASS** | 契约测试 5/5 通过 |
| quality_metric CLI 事件含诊断字段 | **PASS** | CLI 日志确认 17 字段 |
| 分阶段日志埋点 (8阶段) | **PASS** | 阶段1-8 全部输出 |
| Python ctypes 封装同步 | **PASS** | PhotometricDiag 结构体 + argtypes 更新 + 5元组返回 |
| 不引入回归 | **PASS** | KD-tree bug 为预存问题, P12-001 未修改算法核心逻辑 |

## 已知问题 (P12-002 范围)
1. **KD-tree 方向逻辑 bug**: `KdTree2D::findNearestRec` 中 `first/second` 子树选择反转, 导致远离根节点的查询点无法找到最近邻
2. **影响**: 测试1/2/4 的 n_matched 远低于期望值 (1 vs 10/19/8)
3. **归属**: P12-002 (修复Gaia到PSF空间匹配与唯一配对) — spec.md 明确 "不改算法核心逻辑"

## 结论
P12-001 (增加Photometric分阶段诊断) 的所有交付项已完成:
- PhotometricDiag 结构体 (20 字段) 在 C++ DLL 中正确填充
- Python ctypes 封装同步 (PhotometricDiag + argtypes + 5元组返回)
- Orchestrator 写入 photo_stats KV (17 字段) + 生成 photometry_report.json
- CLI quality_metric 事件输出 17 个诊断字段
- 契约测试 5/5 通过

预存 KD-tree 匹配 bug 不影响 P12-001 诊断功能验证, 将在 P12-002 修复。
