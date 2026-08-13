# Config / Schema（单一事实来源）

规则：C++ struct 默认值、parser 默认值、JSON schema、template config、
docs、tests 必须一致；一致性由 `tools/config_consistency_check.py` 校验
（V14 交付）。

## Stage2 config 段

```text
inputs.hips[] / target_order
model: control_grid_per_tile(8) patch_radius_leaf(2) min_samples(5)
       snr_search_radius_deg(0.05)
       background_patch_radius(8) background_clip_sigma(3.0)
       background_clip_iters(3) background_max_contamination(0.20)
       background_contamination_sigma(3.0)
       background_min_retained_fraction(0.60)
       background_tolerance(3.0) background_neighbor_radius(2)
       background_catalog_veto(1)
       huber_delta(1.345) smoothing(auto→0.1) zero_anchor_weight(1e-3)
       max_irls_iterations(100) tolerance(1e-6) sigma_floor(1e-3)
       support_power(1.0) robust_loss(huber) snr_weight_mode(snr2_normalized)
integration: precision(fp32) memory_limit_mb rejection{sigma low 4 high 3
             max_iterations 8 min_samples 2} weight_mode(auto)
             acr_route(cpu/auto)
output.hips / diagnostics
```

默认值来源：`stage2_common.h`（C++ struct）与 `stage2_common.cpp`
（parser）为唯一双实现，consistency test 保证一致。

## Stage1 config

见 `lib/orchestrator/configs/stage1_*.json` 模板。
