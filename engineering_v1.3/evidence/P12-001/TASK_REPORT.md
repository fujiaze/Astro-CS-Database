# P12-001 任务报告 — 增加Photometric分阶段诊断

## 任务信息
- **任务ID**: P12-001
- **阶段**: P12 (Photometric 分阶段诊断)
- **Gate**: G12
- **依赖**: P11-006 (COORDINATE_CONTRACT_V2+CLI_CAPABILITIES+PROVENANCE)
- **参考**: `docs/06_PHOTOMETRY_CORRECTION_SPEC.md`
- **执行日期**: 2026-07-28
- **子任务C范围**: 同步 Python ctypes 封装 + 运行测试 + 生成证据文件

## 目标
在测光模块各阶段 (Fsyn/投影/候选/匹配/拒绝/拟合/残差) 埋点计数, 通过结构化出参返回, 由 Orchestrator 序列化为 JSON 诊断文件 + CLI quality_metric 事件 + photo_stats KV 块。

## 执行摘要

### 子任务C 完成项
1. **Python ctypes 封装同步** (`lib/photometric_calib/python/photometric_calib.py`)
   - 新增 `PhotometricDiag` ctypes 镜像结构体 (20 字段, 与 C++ 端一一对应)
   - 新增 `to_dict()` 方法供日志/JSON 序列化
   - 更新 `_setup_signature()`: `pc_calibrate_simple` 和 `pc_calibrate_simple_with_gaia` 的 argtypes 末尾新增 `POINTER(PhotometricDiag)` 出参
   - 更新 `calibrate_simple()` 和 `calibrate_with_gaia()` 返回 5 元组 `(out_pixels, n_matched, scale, sigma_residual, diag)`
   - DLL 加载增强: 显式 `os.add_dll_directory` + MinGW bin 路径 + 预加载 `gaia_client*.dll` 二级依赖

2. **测试更新** (`lib/photometric_calib/cpp/test/test_photometric_calib.py`)
   - 现有测试 (1-4) 更新为接受 5 元组返回值
   - 新增 `test_diag_output()` 验证 PhotometricDiag 字段:
     - psf_total/psf_valid/fit_used > 0
     - diag.scale_factor ≈ scale (1e-9 容差)
     - diag.sigma_residual ≈ sigma_residual (1e-9 容差)

3. **契约测试** (`engineering_v1.3/evidence/P12-001/scripts/test_contract.py`)
   - jsonschema.validate 验证 photometry_report.json 符合 schema
   - required 字段齐全性 (9 个)
   - P12-001 诊断字段齐全性 (10 个额外)
   - 字段类型正确性
   - match_distance 子字段齐全性

4. **Orchestrator CLI 验证**
   - photometry_report.json 生成成功 (含 17 个诊断字段)
   - quality_metric CLI 事件输出 (含 17 个诊断字段)
   - 8 个阶段日志埋点全部输出

### 测试结果
| 测试类别 | 通过/总计 | 说明 |
|----------|-----------|------|
| 单元测试 | 2/5 | 3 个 FAIL 因预存 KD-tree bug (P12-002 范围) |
| 契约测试 | 5/5 | 全部通过 |
| CLI 验证 | 3/3 | photometry_report + quality_metric + 日志埋点 |

### 已知问题 (P12-002 范围)
- **KD-tree 方向逻辑 bug**: `KdTree2D::findNearestRec` 中 `first/second` 子树选择反转
- **影响**: 10 颗 PSF 星中仅 1 颗匹配成功 (期望 10)
- **根因**: `diff = p[axis] - query[axis]`, `diff < 0` 时 query 在右侧应探索 right, 代码探索 left
- **P12-001 无回归**: git diff 确认 P12-001 未修改 `KdTree2D` 类, 仅新增 diag 埋点
- **归属**: P12-002 (spec.md 明确 "不改算法核心逻辑")

## 修改文件清单

### 子任务C (Python 封装同步)
| 文件 | 修改内容 |
|------|----------|
| `lib/photometric_calib/python/photometric_calib.py` | 新增 PhotometricDiag 结构体 + to_dict() + argtypes 更新 + 5元组返回 + DLL 加载增强 |
| `lib/photometric_calib/cpp/test/test_photometric_calib.py` | 现有测试 4→5 元组 + 新增 test_diag_output |
| `engineering_v1.3/evidence/P12-001/scripts/test_contract.py` | 新增契约测试脚本 (5 项验证) |

### 子任务A/B (已完成, 本子任务未修改)
| 文件 | 修改内容 (子任务A/B) |
|------|----------------------|
| `lib/photometric_calib/cpp/include/photometric_calib.h` | PhotometricDiag 结构体 + 出参声明 |
| `lib/photometric_calib/cpp/src/pc_api.cpp` | diag 填充 + 透传 |
| `lib/photometric_calib/cpp/src/star_matcher.h` | matchAndClean diag 出参 |
| `lib/photometric_calib/cpp/src/star_matcher.cpp` | 8 阶段埋点 + percentileOf + initDiag |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | photo_stats KV (17字段) + photometry_report.json |
| `lib/orchestrator/cpp/src/cli_command.cpp` | quality_metric CLI 事件 |

## PhotometricDiag 结构体 (20 字段)
```cpp
struct PhotometricDiag {
    // 阶段1: Fsyn
    int spectrum_rows_total;      // n_gaia
    int valid_fsyn;               // f_syn > 0 且有限
    // 阶段2: 投影
    int gaia_projected_in_frame;  // 投影后落在 [0,W)×[0,H)
    // 阶段3: PSF
    int psf_total;
    int psf_valid;                // status==0
    // 阶段4/5: 匹配
    int spatial_candidates;       // KD-tree 查询命中
    int unique_matches;           // 唯一配对后
    // 阶段6: 拒绝原因
    int rejected_ambiguous;       // 双向匹配冲突 (当前 0)
    int rejected_distance;        // 距离超阈值
    int rejected_quality;         // F<=0/非有限/星等不一致/IRLS离群
    // 阶段7: 拟合
    int fit_used;                 // IRLS inliers
    int robust_iterations;        // IRLS 迭代次数
    double scale_factor;
    double sigma_residual;
    // 阶段8: 残差/距离统计
    double r_median, r_p90, r_max;
    double match_distance_median, match_distance_p90, match_distance_max;
};
```

## 通过条件
1. ✅ PhotometricDiag 结构体正确填充 (测试5 + CLI 日志)
2. ✅ photo_stats KV 块含 17 个诊断字段
3. ✅ photometry_report.json 符合 schema (契约测试 5/5)
4. ✅ quality_metric CLI 事件含 17 个诊断字段
5. ✅ 分阶段日志埋点 (8 阶段)
6. ✅ Python ctypes 封装同步
7. ✅ 不引入回归 (KD-tree bug 为预存问题)
8. ⚠️ fit_used ≥ 20 (broadband) / ≥ 8 (窄带) — 受 KD-tree bug 影响, P12-002 修复后满足
9. ⚠️ sigma_residual 有限且 >0 — 受 KD-tree bug 影响 (单点 IRLS 无散布), P12-002 修复后满足

## 后续
- P12-002: 修复 KD-tree 方向逻辑 bug + Gaia 到 PSF 空间匹配与唯一配对
- P12-003: 验证光谱积分与响应曲线无回归
- P12-004: T1-T4 与滤镜类别测光矩阵验证
