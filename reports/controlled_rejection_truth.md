# Controlled Clean Rejection Truth（V17）

## 构造

```text
known sky（0.01）
+ 2 个 faint extended Gaussian 结构（amp 0.02/0.015）
+ 8 个 PSF 星（amp 0.3-0.9，FWHM 2.5-4.0px）
+ 独立 Gaussian 噪声（σ=0.002）× 20 帧（零 outlier）
→ 20 个 per-exposure HiPS（单 order-7 tile，support=1 全覆盖，
  MOC/properties/snr catalogue 完整）
→ 生产 stage2：truth(none) 与 auto(wbpp_2_9_1, astrocs_median_center_v1)
```

## 指标（evidence/controlled_rejection_metrics.json）

| 指标 | 值 |
| --- | --- |
| true sample FPR | **1.88%**（565/30000） |
| pixel any-rejection FPR | 26.3%（1500 背景像素） |
| Siril 1.4.3 frozen harness 同源 case 一致率 | **100%**（8000 decisions 逐样本相同；Siril 自身同样拒 1.8375%） |
| star aperture flux rel bias | −0.07% |
| PSF FWHM rel bias | +0.012% |
| faint structure rel bias | −0.29% |
| background noise efficiency（auto/none） | 1.045 |
| thin satellite recall | 1.0（1145 px） |
| compact cosmic recall | 1.0（96 px） |
| hot streak recall | 1.0（2884 px） |
| satellite mosaic band mean diff | −5.9e-05（≈0） |

## 结论

1. LinearFit 的 1.88% FPR 是 **frozen Siril reference 行为**（同源 case
   100% 一致），不是 AstroCS 过拒；
2. 星点通量/PSF/结构保持（<0.3% 偏差），背景噪声效率 ~1.05；
3. 三类注入 outlier recall 全 1.0；
4. true FPR 只能由零离群 truth 测量；真实 16 帧只报 observed rate。

```text
CONTROLLED_REJECTION_TRUTH = PASS
```
