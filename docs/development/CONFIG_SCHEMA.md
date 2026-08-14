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
integration: precision(fp32) memory_limit_mb rejection{method
             none|sigma|winsorized_sigma|averaged_sigma|linear_fit|
             generalized_esd|rcr|percentile|median_sigma|minmax|auto
             profile(wbpp_current) underdetermined_n(2)
             robust_mad_clip{lower_sigma 4 upper_sigma 3 max_iterations 8}
             winsorized_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             averaged_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             linear_fit{lower 4 upper 3 max_iterations 8}
             generalized_esd{alpha 0.05 max_outliers 10}
             percentile{low_fraction 0.1 high_fraction 0.1}
             median_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             minmax{reject_low_count 1 reject_high_count 1
                    max_iterations 8 min_kept 4}
             rcr{technique ss_median_dl}
             low/high/max_iterations/min_samples DEPRECATED（V15 adapter）}
             weight_mode(auto) acr_route(cpu/auto)

rejection.method 说明（V15 Final Semantic Closure）：
  - production 默认 `method=auto` + `profile=wbpp_current`；
  - auto 在 **planning 层**按 integration cohort/tile 的 nominal
    contributors（几何可贡献独立 exposure 数）解析一次，禁止在 pixel loop
    内按 effective count 路由；WBPP 2.9.1（本机源码 bestRejectionMethod）：
      nominal<6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit；
  - effective 候选数 <= underdetermined_n（默认 2）或 < 方法 minimum N →
    REJECTION_UNDERDETERMINED（可全接受但必须记录，禁止偷偷换算法）；
  - percentile: 相对 median 的百分比 clip（low_fraction/high_fraction
      为小数，如 0.1/0.3）；
  - median_sigma: median 位置 + SD 尺度迭代 clip（WBPP Median Sigma）；
  - minmax: 每轮剔除最小/最大样本（WBPP Min/Max）；
  - sigma = astrocs.robust_mad_clip.v1（median + MAD 迭代 clip；
      Astropy sigma_clip(mad_std) oracle）；旧字符串 "sigma" 为 alias；
  - winsorized_sigma: robust 版（median 位置 + 1.5σ winsorize 迭代，
      对齐 Siril 1.4.3 rejection_float.c）。
  - 旧顶层 low/high/max_iterations/min_samples 仅经 deprecation adapter
      映射（打印 warning），生产新配置必须使用 method-specific typed 参数。
output.hips / diagnostics
```

默认值来源：`stage2_common.h`（C++ struct）与 `stage2_common.cpp`
（parser）为唯一双实现，consistency test 保证一致。

## Stage1 config

见 `lib/orchestrator/configs/stage1_*.json` 模板。
