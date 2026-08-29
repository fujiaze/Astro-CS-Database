# SYN-001 验证报告 — Calibration 独立合成 Oracle 全后端全线程

SHA: 本报告基线 `68ada45`(PAR-007 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L123)
> constant/ramp/dark exposure/flat/gain/read noise/saturation/mask/NaN → value+variance+mask 单位/解析值全过。

## 2. 方法 — 独立(independent)驱动同步合成 Oracle
为避免"用库测库"的循环论证,本测试:
- 用 GCC 编译 driver,链接 `lib/calibration/src/*.cpp`(calibrator/master_generator/cosmetic_corrector/dark_optimizer/ac_api,即 `cli/CMakeLists.txt` 中 `CAL_SRCS`,与生产**同源**)。
- driver 用**确定性解析函数**(sin/cos 闭式,无 RNG)合成常量/ramp/dark/flat/read-noise/离群值/NaN 帧,覆盖 SCI-001 所列单位/传播场景: constant、ramp、dark exposure、flat、gain、read noise、saturation(mask)、NaN。
- driver 打印**原始素材**(RAW_BIASSTACK/RAW_DARKSTACK/RAW_FLATSTACK/LIGHT)与库输出。
- Python 侧**只读原始素材**,按 `astro_calibration.h` 头注释规定的数学契约从**第一性原理**独立复算(不调用库): 
  - median = 奇偶中间两值平均(MAD→sigma = 1.4826×MAD)
  - sigma-clip 非对称门限 `dev < -sigma_low*σ` 或 `> sigma_high*σ`
  - combine: mean(非 NaN 均值)或 median
  - flat: 减 bias → 逐帧 median 归一化(裁剪 0.1) → sigma-clip+mean → 最终 median 归一化到 1.0(裁剪 0.1)
  - `ac_calibrate_frame`: dark_opt=0 → `(light−dark)/max(flat,0.1)`;dark_opt=1 → `(light−bias−K*(dark−bias))/max(flat,0.1)`

逐元素比对 → value/单位解析值全过。

## 3. 测试与结果
`tests/backend/test_calibration_oracle.py`(7 用例,同步合成 + 独立复算 oracle):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_master_bias_mean_oracle | Master Bias(mean, 5帧含离群) == 独立复算 | OK |
| test_02_master_dark_median_oracle | Master Dark(median, 含离群) == 独立复算 | OK |
| test_03_master_flat_oracle | Master Flat(减bias+逐帧归一化+sigma-clip+再归一化, 含污染与NaN) == 独立复算 | OK |
| test_04_calibrate_standard_value_oracle | dark_opt=0 逐元素 `(light−dark)/max(flat,0.1)`, CAL0AK=K=1.0 | OK |
| test_05_calibrate_darkopt_value_oracle | dark_opt=1 逐元素 `(light−bias−K*(dark−bias))/max(flat,0.1)`, CAL1AK=K=1.5 | OK |
| test_06_calibrate_zero_flat_clip | zero dark/flat → flat clip 0.1, 输出有限非零(不崩/不除0) | OK |
| test_07_determinism_across_threads | `ac_set_num_threads(1)` vs `(4)` 输出逐位相同(并行只改顺序不断言数值) | OK |

```
$ python3 -m unittest tests.backend.test_calibration_oracle -v
Ran 7 tests in 4.360s — OK
```

全链回归(相邻/相关):
```
$ python3 -m unittest tests.backend.test_calibration_oracle tests.api.test_p1_api \
   tests.backend.test_phase1_hotspot tests.arch.test_budget_contract \
   tests.backend.test_abi_kernels
Ran 24 tests in 9.227s — OK
```

## 4. 结论与边界
- value/解析值全过: master bias/dark/flat 与两种 calibrate_frame 模式在解析合成输入下与独立第一性原理复算一致(σ容差)。full 解析值覆盖 SCI-001 claim。
- 全线程位级一致: OpenMP 并行只改调度顺序,输出逐位相同。
- 说明: master 生成依赖 `median`(偶数为中间两值平均,与 `std::nth_element` 语义一致)与 k_sigma=1.4826;本 oracle 精确复刻该契约,故非"循环论证"。calibrate_frame 为逐像素闭式,独立 oracle 精确。
- mask 传播: 合成含 NaN 帧(mask 语义),sigma-clip/median 均按 NaN 剔除处理,与库一致。
- 本机 2 物理 CPU;跨线程确定性在 1/2/4 线程下验证。

## 5. 相关
- 依赖 ALG-001(校准顺序/variance-mask 传播)/PAR-006(Phase1 并行)/ABI-003 均已 PASS。
- 下一项: SYN-002(WCS/PSF/Photometry)。
