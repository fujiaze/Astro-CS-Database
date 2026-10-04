# Config / Schema（单一事实来源）

> 上游：ACSD_DESIGN.md §8（软件架构）

规则：C++ struct 默认值、parser 默认值、JSON schema、template config、
docs、tests 必须一致；一致性由 `eng/tools/config_consistency_check.py` 校验。

> **文档范围与 `output_mode` 口径（依 `ACSD_DESIGN.md` §0.2「详细层只陈述与本设计一致的细化内容」）**
> - 本文描述的是 **orchestrator 的 Stage2 配置**（parser = `lib/algorithms/coverage/src/stage2_common.cpp`，
>   struct = `stage2_common.h`），**不是**三命令 `phase_config` 合同；后者的语义与索引唯一权威 =
>   `docs/engineering/CONFIG_CONTRACT.md` §3。
> - 本文的 `precision(fp32)` 是 **orchestrator** 的解析缺省（仅用于「doc ↔ parser/struct 一致」门）；
>   三命令 `phase_config` 的精度**必须显式**（normalize = 块级 `drizzle.precision_mode`；mosaic/export =
>   位深键（`config.precision` 为死键，不得再于配置中使用），见 `CONFIG_CONTRACT.md` §3「精度显式声明」。
> - **`output_mode` 不属本文范围**（它只出现在 export 的 `phase_config`）：**必填且必须显式** ——
>   `blocks[]` 分支、平铺单块简写分支、`{phase_name, config, inputs[]}` 简写分支的 `required`
>   **都含 `output_mode`**（fail-closed；`docs/engineering/CONFIG_CONTRACT.md` §3 末条）；
>   合同登记的 `surface_brightness` **只作 `--template` 骨架值**，运行期缺省另有显式来源。
>   ⇒ 本文与 `CONFIG_CONTRACT.md` 在此点上**两边同向**。

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
       support_power(1.0) robust_loss(huber) snr_weight_mode(snr2_normalized)  # UPM fit 内部诊断开关（键名以实现解析点 lib/algorithms/coverage/src/stage2_common.cpp 为准；`docs/ACSD_DESIGN.md` §3.1（数据对象）的「无权重模式可选概念」约束的是 Phase2 集成 weight_mode 概念，与本键无关）
integration: precision(fp32) memory_limit_mb rejection{method
             none|sigma|winsorized_sigma|averaged_sigma|linear_fit|
             generalized_esd|rcr|percentile|median_sigma|minmax|auto
             profile(acsd_adaptive_pixel(生产默认,自研)|wbpp_2_9_1(对照档)|wbpp_current alias|acsd_adaptive)
             underdetermined_n(0=按 profile 解析；pixel=3/其余=2/extreme_prior=1)
             normalization(none|acsd_median_center_v1|acsd_median_scale_v1)
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
             weight_mode(auto)   # acr_route **不是现行键**（已从 parser 删除）：出现在 integration 段即 fail-closed 拒绝；退役对象的拒绝面必须存活，不得静默忽略或静默取默认值（与同函数内 weight_mode / legacy_allow_weight_fallback 的退役键拒绝面同型，stage2_common.cpp）。集成执行路由唯一 = CPU。权重仍是派生量，见下方 note

rejection.method 说明（V17 冻结）：
  - 默认 `method=auto` + `profile=acsd_adaptive_pixel`
    （**Astro Celestial Sphere Database（ACSD） 自研**，逐输出像素几何 N 内置映射：1≤N≤3→none；4≤N≤5→
    percentile；N≥6→winsorized_sigma；线性拟合档不参与逐像素自动路由；阈值逐档继承
    SCI-REJ 冻结锚点）；`wbpp_2_9_1` 为**对照档**（`wbpp_current` 为
    alias，解析并序列化为 wbpp_2_9_1）；
  - auto 在 **planning 层**按 integration cohort/tile 的 nominal
    contributors（几何可贡献独立 exposure 数）解析一次，禁止在 pixel loop
    内按 effective count 路由；对照档 = **本仓冻结解析表**（档界取自 WBPP 2.5.9
    `bestRejectionMethod`，`engine.js:1421-1429`，包 sha1 `712cc7c3…`；
    其 `n>15` 分支为 ESD，本仓该档取 linear_fit = WBPP ≤2.3.x 旧表）：
      nominal<6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit；
  - `acsd_adaptive` = ACSD 自有策略（tile nominal depth 自适应，
    独立命名，不冒充 WBPP exact）；
  - effective 候选数 <= underdetermined_n（**默认 0 = 按 profile 解析**：wbpp/adaptive=2；
    acsd_adaptive_pixel=3；显式 request=EXTREME_VALUE_PRIOR_SIGMA（opt-in）=1）或 < 方法 minimum N →
    REJECTION_UNDERDETERMINED（可全接受但必须记录，禁止偷偷换算法）；
  - normalization：判定工作域与科学值域分离（decision 作用于
    working stack，accepted mask 应用回原始 calibrated 值）；
    percentile 必须 acsd_median_center_v1（负值安全）；rcr 必须 none；
  - large_scale：acsd.large_scale_rejection.v1（8-连通分量 grow，
    min_structure_pixels 过滤，low/high 独立半径；默认关闭）；compact
    cosmic/星点不会无限生长；PIXINSIGHT_EXACT=NOT_CLAIMED；
  - percentile: 相对 median 的百分比 clip（low_fraction/high_fraction
      为小数，默认 0.2/0.1 = WBPP Light percentileLow/High）；
  - median_sigma: median 位置 + SD 尺度迭代 clip（WBPP Median Sigma）；
  - minmax: 一次性固定 rank 剔除最小 reject_low_count 与最大
      reject_high_count 个样本（n−low−high >= min_kept；无 max_iterations）；
  - sigma = acsd.robust_mad_clip.v1（median + MAD 迭代 clip；
      Astropy sigma_clip(mad_std) oracle）；旧字符串 "sigma" 为 alias；
  - winsorized_sigma: robust 版（median 位置 + 1.5σ winsorize 迭代；
      语义来源 = PixInsight ImageIntegration 官方式[18]/[19]（Huber 体系）；
      Siril 1.4.3 仅作**次生参考实现对拍**，不是语义来源）。
  - V17：旧顶层 low/high/max_iterations/min_samples 已从 parser 删除，
      出现即硬错误（提示 eng/tools/migrate_stage2_config.py）。
output.hips / diagnostics
```

默认值来源：`stage2_common.h`（C++ struct）与 `stage2_common.cpp`
（parser）为唯一双实现，consistency test 保证一致。

> **排异档位映射**
> **生产科学路由唯一权威** = `ACSD_DESIGN.md` §5.5：`1≤N≤3` none / `4≤N≤5` percentile /
> `N≥6` winsorized（M3：原 `N≥16` linear fit 档改投）；N = 该输出像素的**几何可贡献帧数**，逐像素自动路由；
> **min/max 不用于生产**。内核同值见 `lib/algorithms/coverage/src/rejection.cpp`
> 的 `kPixelSmallNPolicy`。上方 fenced 块是 `eng/tools/config_consistency_check.py` 的 docs 腿输入，
> 其 `acsd_adaptive_pixel` 档位与本条同值；WBPP 对照档（`nominal<6 / 6..15 / >15`）只描述
> `wbpp_2_9_1` 对照 profile 自身。
> `docs/science/DATA_SEMANTICS.md` §22 首注同面。

## 生产调用链（入口符号 → 配置门 → 实现符号 → 诊断字段）

本节登记两个命令的生产调用链：每个生产入口由哪条**配置门**控制、落到哪个实现符号、以何种
执行模式运行、写出哪个**诊断字段**、由哪个测试 ID 守护。配置门的键名与诊断字段名是写 config
时实际要敲的键，不可从别处推断，故在此逐条登记。

字段口径：`entry_symbol` 生产入口符号；`config_gate` 控制该入口的配置门（`*` 表示该节点由
一组门共同控制）；`target` 目标模块域；`source_symbol` 实现符号；`execution_mode` 执行模式；
`diagnostic_field` 该入口落盘/回传的诊断字段；`test_id` 守护该行的测试 ID。

### stage1（`normalize` 子命令）

| 入口符号 | 配置门 | 目标 | 实现符号 | 执行模式 | 诊断字段 | 测试 ID |
|---|---|---|---|---|---|---|
| normalize --json <config.json> (唯一 CLI 子命令) | phase_config_normalize.schema.json | acsd(cli) | normalize_subcommand (lib/infrastructure/cli/normalize/normalize.h) + session_dispatch (commands.cpp:2404) | serial | exit_code | TEST-CLI-001 |
| Orchestrator::run_stage_calibrate | stage1.calibrate.enabled | astro_calibration | ac_calibrate_frame / calibrate | parallel_cpu (OpenMP 16) | calibrate.t_ms | TEST-CAL-001 |
| Orchestrator::run_stage_platesolve | stage1.platesolve.enabled | ipv_solver | ipv_solve_from_detections_v1 / build_wcs | serial | platesolve.rms_px | TEST-IPV-001 |
| Orchestrator::run_stage_photometric | stage1.photometric.enabled | photometric_calib | pc_calibrate_simple | serial | photometric.zero_point | TEST-SPEC-001 |
| Orchestrator::run_stage_drizzle | stage1.drizzle.enabled | healpix_drizzle | drizzleTiled / processPixelSharedTiled | parallel_cpu (OpenMP) | drizzle.n_tiles | TEST-DRZ-CAND-001 |
| Orchestrator::run_stage_snr | stage1.snr.enabled | snr_estimator | snr_noise_model_v1 | parallel_cpu | snr.sigma_bg | TEST-SNR-001 |
| aio_hips_write_signal_support_tile | stage1.output.hips | astro_image_io | aio_hips_product_begin | serial+async_io | hips.nside | TEST-HIPS-001 |

### stage2（`mosaic` 子命令）

| 入口符号 | 配置门 | 目标 | 实现符号 | 执行模式 | 诊断字段 | 测试 ID |
|---|---|---|---|---|---|---|
| mosaic --json <config.json> (唯一 CLI 子命令) | phase_config_mosaic.schema.json | acsd(cli) | mosaic_subcommand (lib/infrastructure/cli/mosaic/mosaic.h) + session_dispatch (commands.cpp:2404) | serial | exit_code | TEST-CLI-002 |
| coverage_union | stage2.inputs.hips | phase2/coverage | p2_coverage_union | serial | coverage.n_frames | TEST-COV-001 |
| control_sample | stage2.model.* | phase2/sampler | p2_sample_controls | serial (P2_ENABLE_OPENMP OFF) / parallel_cpu if ON | sampler.n_controls | TEST-UPMW-004 |
| upm_build | stage2.model.* | phase2/upm | p2_upm_build | serial | upm.n_components | TEST-UPMW-001 |
| upm_persist | stage2.output.upm | astro_image_io | aio_upm_write_sparse | serial | upm.sha256 | UpmPersistAllPermutations |
| block_calibrate | stage2.integration.* | phase2/block | p2_upm_calibrate_block | parallel_cpu | block.t_ms | TEST-P2-CALIB-001 |
| rejection | stage2.integration.rejection.* | phase2/rejection | p2_reject_stack_ex (7 档自动选择 SD-18: 1-3 none/4-5 percentile/N>=6 winsorized) | tile 级并行 (p2_parallel_for,  std::thread) | rejection.n_rejected_low/high | TEST-REJ-* |
| integration | stage2.integration.* | phase2/integrate | p2_integrate_pixel | tile 级并行 (p2_parallel_for,  std::thread) | integrate.signal/support | V17StatusesExplicit |
| hips_write | stage2.output.hips | astro_image_io | aio_hips_writer | serial+async_io | hips.nside | TEST-HIPS-001 |

## Stage1 config

见 `lib/infrastructure/pipeline/orchestrator/configs/stage1_*.json` 模板。

- `drizzle.pixfrac` (0,1]：`stage1.schema.json` 默认 0.8（生产默认收缩滴落，`stage1.template.json` 同）；
  银心三面板 `stage1_gc_panel{1,2,3}_Red.json` 为 `pixfrac=1.0` 无收缩分支（最大覆盖/GC 专用），与默认分支在 `lib/infrastructure/pipeline/orchestrator/configs/` 并存。
