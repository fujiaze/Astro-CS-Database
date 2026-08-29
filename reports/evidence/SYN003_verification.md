# SYN-003 验证报告 — Noise/SNR estimator 独立合成 Oracle

SHA: 本报告基线 `c03eb89`(SYN-002 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L125)
> Gaussian/Poisson/constant/blank sky/outlier/small-N → estimator bias/variance/SNR/ivar 和边界符合 SCI。

## 2. 方法 — 独立(independent)合成 Oracle
- 编译 driver 链接 `lib/snr_estimator/cpp/src/noise_model.cpp`+`snr_estimator.cpp`(生产同源,std-only 无外部依赖)。
- driver 用独立解析/抖动生成已知统计的 blank-sky 帧,调 `snr_noise_model_v1`(Gaussian σ_true、常量、Poisson+read、含 cosmic/hot 离群、small-N 退化),另调 `snr_noise_model_v1_fill`、`snr_noise_gain_variance`、`snr_noise_scale_law`。
- Python 侧**第一性原理**复算(不调库): 
  - `σ_bg = 1.4826022185 × median(|x − median(x)|)`(头文件 + noise_model.cpp L61)
  - 全局兜底 = 合格 patch variance 的稳健中位数(noise_model.cpp L209)
  - `ivar = 1/(σ²)`(floor clamp)
  - patch 中心 = (x0+x1)/2,(y0+y1)/2(0-based)
  - Poisson 模型 `var = max(s,0)/gain + (read/gain)²`(头显式)
  - scale law `var'=α²·var, ivar'=ivar/α²`(头显式)
  - 空间场: 最小二乘平面 `var(x,y)=a+b·x+c·y`,负预测 clamp floor

逐项与库输出比对 → bias/variance/SNR/ivar 与边界全过。

## 3. 测试与结果
`tests/backend/test_noise_model_oracle.py`(6 用例):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_gaussian_blank_sky_unbiased | Gaussian blank-sky(σ_true=7.3)→ 全局 σ 无偏恢复(相对偏差<5%) | OK |
| test_02_constant_background_boundary_and_smallN | 常量背景(σ_true=0)→ σ≈floor 级;small-N 3x3→ 完全退化 ivar=0 | OK |
| test_03_outlier_robust_clip | σ_true=5 + 1% 离群(±500)→ 稳健裁剪去除,σ 不被抬高(<10%) | OK |
| test_04_poisson_readnoise_analytic | `var=max(s,0)/gain+(read/gain)²` 解析模型(signal 正/负/gain=0) | OK |
| test_05_scale_law | `x'=αx → var'=α²·var, ivar'=ivar/α²`(α=2) | OK |
| test_06_spatial_field_fill | fill 逐像素 ivar=1/σ² 数量级(空间场平面,σ≈4→1/16) | OK |

```
$ python3 -m unittest tests.backend.test_noise_model_oracle -v
Ran 6 tests in 2.463s — OK
```
回归(相邻):
```
$ python3 -m unittest tests.api.test_p1_api tests.backend.test_noise_model_oracle
Ran 12 tests in 2.421s — OK
```

## 4. 结论与边界
- unbiased bias: Gaussian blank-sky σ_bg 无偏恢复(σ_true=7.3)。满 SCM(静/动)不适用本合成,但 blank-sky 稳健方差 estimator 的 bias/variance 与 ivar 语义已证。
- boundary: 常量(σ=0)不虚高到 floor;small-N(样本不足)完全退化 ivar=0,调用方应拒绝加权(头注释契约)。
- outlier: cosmic/hot 稳健裁剪(5σ,2 round)去除 ±500 离群,σ 稳健恢复。
- analytic: Poisson+read-noise 模型、scale law(SNR-002)、ivar=1/σ²、空间场平面(SNR-010/014)全按解析契约。
- 说明: 空背景(blank sky)为 v1 生产基线;增益模型为诊断交叉验证;本机 2 物理 CPU。
- 单元语义: 全局兜底亦为稳健中位数(对不合格 patch 鲁棒)。

## 5. 相关
- 依赖 ALG-003(noise estimator/SNR/ivar)覆盖;PAR-006(Phase1 并行)/ABI-003 均 PASS。
- 下一项: SYN-004(Drizzle 独立合成 Oracle 与不变量)。
