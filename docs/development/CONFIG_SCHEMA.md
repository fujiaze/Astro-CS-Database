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
             profile(wbpp_2_9_1|wbpp_current alias|astrocs_adaptive)
             underdetermined_n(2)
             normalization(none|astrocs_median_center_v1|astrocs_median_scale_v1)
             normalization_floor(1e-12)
             large_scale{enabled(false) min_structure_pixels(8)
                         low_grow_radius_pixels(2)
                         high_grow_radius_pixels(2)}
             robust_mad_clip{lower_sigma 4 upper_sigma 3 max_iterations 8}
             winsorized_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             averaged_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             linear_fit{lower 5 upper 3.5 max_iterations 8}
             generalized_esd{alpha 0.05 max_outliers 10}
             percentile{low_fraction 0.2 high_fraction 0.1}
             median_sigma{lower_sigma 4 upper_sigma 3 max_iterations 8}
             minmax{reject_low_count 1 reject_high_count 1
                    min_kept 4}
             rcr{technique ss_median_dl}
             （low/high/max_iterations/min_samples 已删除（V17 硬错误），
              旧 config 必须 tools/migrate_stage2_config.py 迁移）}
             weight_mode(auto) acr_route(cpu/auto)

rejection.method 说明（V17 True Final Freeze）：
  - production 默认 `method=auto` + `profile=wbpp_2_9_1`（冻结版本；
    `wbpp_current` 仅 migration alias，解析并序列化为 wbpp_2_9_1）；
  - auto 在 **planning 层**按 integration cohort/tile 的 nominal
    contributors（几何可贡献独立 exposure 数）解析一次，禁止在 pixel loop
    内按 effective count 路由；WBPP 2.9.1（本机源码 bestRejectionMethod）：
      nominal<6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit；
  - `astrocs_adaptive` = AstroCS 自有策略（tile nominal depth 自适应，
    独立命名，不冒充 WBPP exact）；
  - effective 候选数 <= underdetermined_n（默认 2）或 < 方法 minimum N →
    REJECTION_UNDERDETERMINED（可全接受但必须记录，禁止偷偷换算法）；
  - normalization：判定工作域与科学值域分离（decision 作用于
    working stack，accepted mask 应用回原始 calibrated 值）；
    percentile 必须 astrocs_median_center_v1（负值安全）；rcr 必须 none；
  - large_scale：astrocs.large_scale_rejection.v1（8-连通分量 grow，
    min_structure_pixels 过滤，low/high 独立半径；默认关闭）；compact
    cosmic/星点不会无限生长；PIXINSIGHT_EXACT=NOT_CLAIMED；
  - percentile: 相对 median 的百分比 clip（low_fraction/high_fraction
      为小数，默认 0.2/0.1 = WBPP Light percentileLow/High）；
  - median_sigma: median 位置 + SD 尺度迭代 clip（WBPP Median Sigma）；
  - minmax: 一次性固定 rank 剔除最小 reject_low_count 与最大
      reject_high_count 个样本（n−low−high >= min_kept；无 max_iterations）；
  - sigma = astrocs.robust_mad_clip.v1（median + MAD 迭代 clip；
      Astropy sigma_clip(mad_std) oracle）；旧字符串 "sigma" 为 alias；
  - winsorized_sigma: robust 版（median 位置 + 1.5σ winsorize 迭代，
      对齐 Siril 1.4.3 rejection_float.c）。
  - V17：旧顶层 low/high/max_iterations/min_samples 已从 parser 删除，
      出现即硬错误（提示 tools/migrate_stage2_config.py）。
output.hips / diagnostics
```

默认值来源：`stage2_common.h`（C++ struct）与 `stage2_common.cpp`
（parser）为唯一双实现，consistency test 保证一致。

## Stage1 config

见 `lib/orchestrator/configs/stage1_*.json` 模板。

- `drizzle.pixfrac` (0,1]：`stage1.schema.json` 默认 0.8（生产默认收缩滴落，`stage1.template.json` 同）；
  银心三面板 `stage1_gc_panel{1,2,3}_Red.json` 为 `pixfrac=1.0` 无收缩分支（最大覆盖/GC 专用），与默认分支在 `lib/orchestrator/configs/` 并存，`docs/ARCHITECTURE.md §6` 同步说明。
