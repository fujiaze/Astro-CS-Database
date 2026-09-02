> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/science/、docs/algorithms/

# AstroCS 科学算法索引 (V19)

| 算法 | 位置 | 公式/语义 | Oracle/测试 |
|---|---|---|---|
| 校准 | lib/calibration | bias/dark/flat/cosmetic | CAL-001..006 |
| WCS+TAN/SIP | lib/plate_solve, wcs_sip.cpp | 前向 A/B + CD | AST-001..008 |
| PSF Moffat4 | lib/dynamic_psf | I=B+A/(1+Q)⁴, Q=0.5r²/σ² | PSF-001..008 |
| 测光定标 | lib/photometric_calib | r=log10(F_i/F_syn) dex | PHOT-001..007 |
| 噪声模型 | lib/snr_estimator/noise_model.cpp | blank-sky MAD → ivar | SNR-001..015 |
| Drizzle | lib/healpix_db/healpix_drizzle | 球面 S-H 重叠 + 守恒 | DRZ-001..016 |
| 方差传播 | drizzle_engine.cpp | Σv·w²/Σa² | SNR-011/012 |
| UPM | lib/phase2/upm.cpp | Huber IRLS + 弱零锚 | UPM-001..010 |
| 排异 | lib/phase2/rejection.cpp | 13 方法 + WBPP 策略 | REJ 矩阵 |
| 积分 | lib/phase2/integrate.cpp | ivar 加权 + support max | INT-001..008 |

每个算法文档要求: 公式、变量/单位、假设、有效域、失败模式、源码入口、
oracle/test、参考文献 (见 SNR_NOISE_MODEL.md 示例)。
