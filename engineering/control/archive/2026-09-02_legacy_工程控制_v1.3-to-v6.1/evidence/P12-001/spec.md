# P12-001 Spec — 增加Photometric分阶段诊断

## 目标
在测光模块各阶段（Fsyn/投影/候选/匹配/拒绝/拟合/残差）埋点计数，通过结构化出参返回，由 Orchestrator 序列化为 JSON 诊断文件 + CLI quality_metric 事件 + photo_stats KV 块。

## 范围
- C++ DLL：新增 PhotometricDiag 结构体出参（向后兼容 nullptr）
- Orchestrator：将 diag 写入 photo_stats KV + photometry_report.json
- CLI 事件：在 stage1 成功路径追加 quality_metric 事件
- 测试：单元测试 + 契约测试 + Gate 验证

## PhotometricDiag 结构体字段
```cpp
struct PhotometricDiag {
    int spectrum_rows_total;      // n_gaia
    int valid_fsyn;               // f_syn > 0 且有限
    int gaia_projected_in_frame;  // 投影后落在 [0,W)×[0,H)
    int psf_total;
    int psf_valid;                // status==0
    int spatial_candidates;       // KD-tree 查询命中
    int unique_matches;           // 唯一配对后
    int rejected_ambiguous;       // 双向匹配冲突（0，当前无双向）
    int rejected_distance;       // 距离超阈值
    int rejected_quality;         // F<=0/非有限/星等不一致/IRLS离群
    int fit_used;                // IRLS inliers
    int robust_iterations;        // IRLS 迭代次数
    double scale_factor;
    double sigma_residual;
    double r_median, r_p90, r_max;
    double match_distance_median, match_distance_p90, match_distance_max;
};
```

## 修改文件清单
1. `lib/photometric_calib/cpp/include/photometric_calib.h` — 新增 PhotometricDiag + 出参
2. `lib/photometric_calib/cpp/src/pc_api.cpp` — 填充 diag，透传给 matchAndClean
3. `lib/photometric_calib/cpp/src/star_matcher.h` — matchAndClean 新增 diag 出参
4. `lib/photometric_calib/cpp/src/star_matcher.cpp` — 各阶段埋点计数
5. `lib/orchestrator/cpp/src/orchestrator.cpp` — run_stage_photometric 写 KV + JSON
6. `lib/orchestrator/cpp/src/cli_command.cpp` — quality_metric 事件
7. `lib/photometric_calib/python/photometric_calib.py` — ctypes 封装同步

## Gate
- Broadband/LRGB: fit_used ≥ 20
- 窄带: fit_used ≥ 8
- sigma_residual 有限且 >0
- photometry_report.json 符合 schema

## 不做
- 不改算法核心逻辑（IRLS/Tukey/KD-tree 不变）
- 不补双向最近邻（rejected_ambiguous 暂保持 0，ADR 另议）
- 不改 pc_calibrate_simple 旧接口
