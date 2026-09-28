# 产物比对报告 (compare_products.py)

- A: /workspace/Astro CS Database/run/PERF-401/out/real16_w1
- B: /workspace/Astro CS Database/run/PERF-401/out/real16_w16
- 模式: byte (rel=0 abs=0)
- 结论: **DIFFER**

## 计数

| 项 | 值 |
|---|---|
| A 文件数 | 3094 |
| B 文件数 | 3094 |
| 公有文件 | 3093 |
| 逐字节一致 | 3077 |
| 仅墙钟差异(已忽略) | 4 |
| 数值在容差内(未忽略) | 0 |
| 实质差异 | 14 |
| 仅 A 有 | 1 |
| 仅 B 有 | 1 |

## 被忽略的差异(墙钟字段)

忽略清单(文本): created_utc, ended_utc, generated_utc, hips_creation_date, hips_release_date, hips_update_date, run_id, started_utc, timestamp_utc
忽略清单(FITS 头): CHECKSUM, DATASUM, DATE, RUNID

- ivar/properties [text] keys=hips_creation_date,hips_release_date — 仅已知墙钟字段差异, 其余逐字节一致
- signal/properties [text] keys=hips_creation_date,hips_release_date — 仅已知墙钟字段差异, 其余逐字节一致
- support/properties [text] keys=hips_creation_date,hips_release_date — 仅已知墙钟字段差异, 其余逐字节一致
- variance/properties [text] keys=hips_creation_date,hips_release_date — 仅已知墙钟字段差异, 其余逐字节一致

## 数值在容差内(未忽略, 仅 --tolerance 模式)

- (无)

## 实质差异

- astrocs_run_6377c66a6289.json [missing_in_b] missing_in_b
- astrocs_run_62ae40479b79.json [extra_in_b] extra_in_b
- alloc_report.json [text] content_mismatch
- alloc_samples.csv [text] content_mismatch
- p2_corrected.json [text] content_mismatch
- p2_final.json [text] content_mismatch
- p2_integrated.json [text] content_mismatch
- p2_rejection.json [text] content_mismatch
- p2_samples.json [text] content_mismatch
- p2_upm_model.json [text] content_mismatch
- resource_summary.json [text] content_mismatch
- resource_timeseries.csv [text] content_mismatch
- run_context.json [text] content_mismatch
- worker_balance.csv [text] content_mismatch

## 容差依据

- float32(IEEE-754 binary32) 有效精度 ~7 位十进制; 单次正确舍入 <=0.5 ulp,
- 相对量级约 6e-8。数值归约(求和/加权平均)顺序变化导致的差异随项数 N 以
- O(sqrt(N)) ulp 增长: N=1e4 时约 100 ulp ~ 6e-6 相对。因此 1e-5 相对容差约等于
- 100 倍单 ulp, 足以覆盖 float32 累加顺序变化, 又远小于任何科学量级差异。
- 此为舍入级口径, **不是科学容差**; 每个产品的科学容差必须单独论证。

## 墙钟字段依据

- 忽略清单(文本): hips_creation_date, hips_update_date, hips_release_date, created_utc, generated_utc, timestamp_utc, run_id, started_utc, ended_utc
-   = IVOA HiPS properties 墙钟元数据 + 仓库报告面时间戳 + 运行实例标识/起止时刻。
- 忽略清单(FITS 头): DATE, RUNID, CHECKSUM, DATASUM
-   = DATE(写入墙钟) + RUNID(逐运行随机标识) + CHECKSUM/DATASUM(由前二者派生的
-     完整性校验和; 数据段仍逐元素比对, 数值变化不会因忽略校验和而漏检)。
- 明确不忽略: DATE-OBS(观测时间, 科学输入确定性字段); 绝对路径(output_dir/
- config_path/output_fits)与 product_sha256(由内容派生) —— 路径差异属运行簿记,
- 由报告显式列出, 不以'墙钟'名义忽略; 比对产品树时应只取产品面。
