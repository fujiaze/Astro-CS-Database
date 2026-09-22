# Config / Schema（单一事实来源）

> 上游：ASTROCS_DESIGN.md §8（软件架构）

规则：C++ struct 默认值、parser 默认值、JSON schema、template config、
docs、tests 必须一致；一致性由 `eng/tools/config_consistency_check.py` 校验。

> **文档范围与 `output_mode` 口径（依 `ASTROCS_DESIGN.md` §0.2「详细层不得与本设计相反」）**
> - 本文描述的是 **orchestrator 的 Stage2 配置**（parser = `lib/algorithms/coverage/src/stage2_common.cpp`，
>   struct = `stage2_common.h`），**不是**三命令 `phase_config` 合同；后者的语义与索引唯一权威 =
>   `docs/contracts/CONFIG_CONTRACT.md` §3。
> - 本文的 `precision(fp32)` 是 **orchestrator** 的解析缺省（仅用于「doc ↔ parser/struct 一致」门）；
>   三命令 `phase_config` 的精度**必须显式**（normalize = 块级 `drizzle.precision_mode`；mosaic/export =
>   位深键 / `config.precision` 键名），见 `CONFIG_CONTRACT.md` §3「精度显式声明」。
> - **`output_mode` 不属本文范围**（它只出现在 export 的 `phase_config`）：**必填且必须显式** ——
>   `blocks[]` 分支、平铺单块简写分支、`{phase_name, config, inputs[]}` 简写分支的 `required`
>   **都含 `output_mode`**（fail-closed；`docs/contracts/CONFIG_CONTRACT.md` §3 末条）；
>   合同登记的 `surface_brightness` **只作 `--template` 骨架值**，**不得**当成「运行期缺省」。
>   ⇒ 本文与 `CONFIG_CONTRACT.md` 在此点上**不得两边相反**。

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
       support_power(1.0) robust_loss(huber) upm_weight_source(snr2_normalized)  # 原 snr_weight_mode，（已按 §9.73 A44 作废：键不存在；权重是派生量）
integration: precision(fp32) memory_limit_mb rejection{method
             none|sigma|winsorized_sigma|averaged_sigma|linear_fit|
             generalized_esd|rcr|percentile|median_sigma|minmax|auto
             profile(astrocs_adaptive_pixel(生产默认,自研)|wbpp_2_9_1(对照档)|wbpp_current alias|astrocs_adaptive)
             underdetermined_n(0=按 profile 解析；pixel=3/其余=2/extreme_prior=1)
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
             （low/high/max_iterations/min_samples 不是现行键（出现即硬错误），
              旧 config 必须 eng/tools/migrate_stage2_config.py 迁移）}
             weight_mode(auto) acr_route(cpu/auto)   # （已按 §9.73 A44 作废：键不存在；权重是派生量）

rejection.method 说明（V17 冻结）：
  - 默认 `method=auto` + `profile=astrocs_adaptive_pixel`
    （**AstroCS 自研**，逐输出像素几何 N 内置映射：1≤N≤3→none；4≤N≤5→
    percentile；6≤N≤15→winsorized_sigma；N≥16→linear_fit；阈值逐档继承
    SCI-REJ 冻结锚点）；`wbpp_2_9_1` 为**对照档**（`wbpp_current` 为
    alias，解析并序列化为 wbpp_2_9_1）；
  - auto 在 **planning 层**按 integration cohort/tile 的 nominal
    contributors（几何可贡献独立 exposure 数）解析一次，禁止在 pixel loop
    内按 effective count 路由；对照档 WBPP 2.9.1（本机源码 bestRejectionMethod）：
      nominal<6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit；
  - `astrocs_adaptive` = AstroCS 自有策略（tile nominal depth 自适应，
    独立命名，不冒充 WBPP exact）；
  - effective 候选数 <= underdetermined_n（**默认 0 = 按 profile 解析**：wbpp/adaptive=2；
    astrocs_adaptive_pixel=3；显式 request=EXTREME_VALUE_PRIOR_SIGMA（opt-in）=1）或 < 方法 minimum N →
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
      出现即硬错误（提示 eng/tools/migrate_stage2_config.py）。
output.hips / diagnostics
```

默认值来源：`stage2_common.h`（C++ struct）与 `stage2_common.cpp`
（parser）为唯一双实现，consistency test 保证一致。

> **排异档位映射**
> **生产科学路由唯一权威** = `ASTROCS_DESIGN.md` §5.5：`1≤N≤3` none / `4≤N≤5` percentile /
> `6≤N≤15` winsorized / `N≥16` linear fit；N = 该输出像素的**几何可贡献帧数**，逐像素自动路由；
> **min/max 不用于生产**。内核同值见 `lib/algorithms/coverage/src/rejection.cpp:1139`
> `kPixelSmallNPolicy`。上方 fenced 块是 `eng/tools/config_consistency_check.py` 的 docs 腿输入，
> 其 `astrocs_adaptive_pixel` 档位与本条同值；WBPP 对照档（`nominal<6 / 6..15 / >15`）只描述
> `wbpp_2_9_1` 对照 profile 自身。
> `docs/contracts/DATA_SEMANTICS.md` §22 首注同面。

## Stage1 config

见 `lib/infrastructure/pipeline/orchestrator/configs/stage1_*.json` 模板。

- `drizzle.pixfrac` (0,1]：`stage1.schema.json` 默认 0.8（生产默认收缩滴落，`stage1.template.json` 同）；
  银心三面板 `stage1_gc_panel{1,2,3}_Red.json` 为 `pixfrac=1.0` 无收缩分支（最大覆盖/GC 专用），与默认分支在 `lib/infrastructure/pipeline/orchestrator/configs/` 并存，`docs/architecture/ARCHITECTURE.md §6` 同步说明。
