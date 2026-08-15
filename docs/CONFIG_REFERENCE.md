# AstroCS 配置参考 (V19)

> 权威模板: `lib/orchestrator/cpp/configs/` 与 `lib/phase2/configs/`。
> 机器一致性: `tools/config_consistency_check.py`。

## stage1_config.json (节选)

```json
{
  "calibration": { "masters": { "bias": "...", "dark": "...", "flat": "..." } },
  "platesolve":  { "gaia_data_dir": "GaiaDR3SP", "sip_order": 3 },
  "psf":         { "fit_radius": 8, "max_iter": 100 },
  "photometric": { "filters_json": "...", "qe_curves_json": "..." },
  "drizzle":     { "nside_strategy": "1x_to_2x_drizzle", "nside_override": 0,
                   "pixfrac": 1.0, "tile_depth": 9, "threads": 0 },
  "precision": "fp32",
  "stop_after": "hips_verify"
}
```

## stage2_config.json (V19 权重默认)

```json
{
  "integration": {
    "weight_mode": "ivar",
    "rejection": { "method": "auto", "profile": "wbpp_current" }
  }
}
```

`weight_mode` 枚举: `ivar` (默认) / `equal` / `support_x_snr2` (legacy)。

## 环境变量

```text
ASTROCS_DRIZZLE_TRACE=<dir>      actual-buffer trace (默认关)
ASTROCS_DRIZZLE_FINE_PROFILE=1   逐像素计时 (默认关)
```

## 参数追踪

49 参数注册表: `工程控制/contracts/config_parameter_registry.csv`
(P03-002, 已冻结)。
