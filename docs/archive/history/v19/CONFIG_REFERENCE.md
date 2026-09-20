> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/development/CONFIG_SCHEMA.md

# AstroCS 配置参考 (V19)

> 权威模板: `lib/infrastructure/pipeline/orchestrator/cpp/configs/` 与 `lib/algorithms/coverage/configs/`。
> 机器一致性: `tools/config_consistency_check.py`。

## stage1_config.json (节选)

```json
{
  "calibration": { "masters": { "bias": "...", "dark": "...", "flat": "..." } },
  "platesolve":  { "gaia_data_dir": "GaiaDR3SP", "sip_order": 3 },
  "psf":         { "fit_radius": 8, "max_iter": 100 },
  "photometric": { "filters_json": "...", "qe_curves_json": "..." },
  "drizzle":     { "nside_strategy": "1x_to_2x_drizzle", "nside_override": 0,
                   "pixfrac": 0.8, "tile_depth": 9, "threads": 0 },
  "precision": "fp32",
  "stop_after": "hips_verify"
}
```

## stage2_config.json (V19 权重默认)

```json
{
  "integration": {
    "weight_mode": "ivar",（已按 §9.73 A44 作废：该概念不存在）
    "rejection": { "method": "auto", "profile": "wbpp_2_9_1" }
  }
}
```

`weight_mode` 枚举: `ivar` (默认) / `equal` / `support_x_snr2` (legacy)。（已按 §9.73 A44 作废：该概念不存在）
`profile` canonical 为 `wbpp_2_9_1` (alias 名仅代码层兼容, 文档统一 canonical)。

## 环境变量

```text
ASTROCS_DRIZZLE_TRACE=<dir>      actual-buffer trace (默认关)
ASTROCS_DRIZZLE_FINE_PROFILE=1   逐像素计时 (默认关)
```

## 参数追踪

49 参数注册表: `工程控制/contracts/config_parameter_registry.csv`
(P03-002, 已冻结)。
