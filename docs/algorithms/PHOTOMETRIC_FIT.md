# Photometric Fit Algorithms (ALG-PHOT)

> 上游 SCI: SCI-PHOT-001  状态: DERIVED (T203 冻结, 2026-08-23)  模块: photometric_calib

## 1 上游 SCI 与输入输出

- 上游: `SCI-PHOT-001` (r=log10(F_instr/F_syn) IRLS Tukey c=4.685, mag_tolerance=3.0)
- 输入: 仪器流量 `F_instr` + 合成流量 `F_syn` (Gaia XP)
- 输出: `PhotometricCalibrationQuality` (sigma_mag, sigma_cal_rel, zero_point) + scale

## 2 离散公式

```text
F1: r_i = log10(F_instr,i / F_syn,i)
F2: S = MAD(r)/0.6745, init location=median(r)
F3: IRLS Tukey: u=(r−location)/(c·S), c=4.685, w=(1−u²)² if |u|<1 else 0, location=Σw·r/Σw, iter≤50 tol=1e-6
F4: sigma_residual = MAD(r_inliers)/0.6745, r_inliers={w>0}
F5: sigma_mag = 2.5·sigma_residual, sigma_cal_rel = ln10·sigma_residual
F6: scale = 10^{−location}
F7: 星等一致性预过滤 |Δ−median(Δ)|>3.0 mag reject where Δ=−2.5·log10(F_instr)−G_Gaia
```

来源: `star_matcher.cpp:21,241-248,478-559`

## 3 伪代码

```text
function photometric_fit(F_instr, F_syn, G_Gaia):
  if n < min_ref → NO_DATA
  Δ_i = −2.5·log10(F_instr)−G_Gaia; median_Δ = median(Δ)
  r_consistent = {i | |Δ_i−median_Δ|≤3.0} → r_i=log10(F_instr/F_syn)
  if S=MAD(r)/0.6745 ==0 → location=median(r) skip IRLS
  else:
    location=median(r); repeat 50×:
      w_i=(1−((r_i−location)/(c·S))²)² if |u|<1 else 0; c=4.685
      new_loc=Σw·r/Σw; if |new−old|<1e-6 break
  sigma_res=MAD({r|w>0})/0.6745; scale=10^{−location}
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `F≤0` / log10 非有限 | skip REJECT |
| `MAD==0` | 跳过IRLS取median |
| `S<0` / n<min | NO_DATA |

## 5 确定性与归约

- 排序 median/MAD 确定性；IRLS 按 r 索引固定顺序加权和，无跨样本归约。

## 6 复杂度

- O(n_ref log n_ref) 排序 + O(n_ref·iter) IRLS

## 7 CPU/GPU

- CPU 单线程；GPU 仅排序可加速，等价门 `location ≤1e-9 dex`。

## 8 参考实现/Oracle

- 合成注入偏移恢复 PHOT-001..007 (scale 已知→location=log10 k)

## 9 容差来源

- location tol 1e-6 (IRLS 收敛)，预冻结。

## 10 关联 ARC/API/TST

- API: `pc_api.h: pc_calibrate_simple, pc_calibrate_simple_with_gaia`
- TST: `TST-PHOT-*` 合成注入/鲁棒
