# AstroCS 数据契约 (V19)

> 与 `docs/contracts/` 既有文档一致, 此处为 V19 增量权威。

## PipelineFrame 命名块 (Stage1)

| 块 | 类型 | 语义 |
|---|---|---|
| data | FLOAT32/64 [H,W] | 校准后像素 |
| header | KV | WCS/SIP/FITS 头 |
| star_det | FLOAT64 [N,6] | 星检测权威块 |
| star_measurements | FLOAT64 [N,15] | star_id 贯通 |
| psf | FLOAT64 [N,9] | status/B/flux/cx/cy/fwhm/A/residual_scale/ecc |
| photometric_match | FLOAT64 [N,6] | 测光匹配状态 |
| photo_stats | KV | SIGMA_RESIDUAL (dex) / NOISE_* |
| snr_model | RAW | legacy 稀疏控制点 (DIAGNOSTIC_ONLY) |
| variance | FLOAT32/64 [H,W] | V19 NoiseWeightModelV1 方差 (ADU²) |
| ivar | FLOAT32/64 [H,W] | V19 逆方差 |

## HiPS 产品 (Phase1)

```text
signal/    Image HiPS  flux_sum/covered_area
support/   Image HiPS  covered_area/A_cell ∈ [0,1]
snr/       Catalogue HiPS (legacy 星点诊断)
variance/  Image HiPS  V19: sumVarNum/sumArea²   (ADU²)
ivar/      Image HiPS  V19: 1/variance
```

## 噪声模型产品语义

```text
variance = 传播方差 (含 drizzle 线性算子的源像素方差贡献)
ivar     = 1/variance (Phase2 积分权重, weight_mode=2)
NaN 语义: covered_area<=0 → variance/ivar NaN (与 signal 一致)
```

## Phase2

```text
UPM:        astrocs-upm-v1 JSON + dense cache (source_hash 校验)
输出:       signal + support Image HiPS (weight/rejection_count 诊断可选)
权重:       stack.ivar.v1 (默认) / equal / support_x_snr2 (legacy)
```
