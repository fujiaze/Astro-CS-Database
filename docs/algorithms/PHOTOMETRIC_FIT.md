# Photometric Fit

关联：SCI-PHOT-001；模块：lib/photometric_calib。

## 输入

仪器流量 + 合成星表流量。

## 输出

PhotometricCalibrationQuality：sigma_mag / sigma_cal_rel / 零点。

## Preconditions

≥N 参考星；流量正有限。

## Postconditions

残差 r=log10(F_instr/F_syn)；稳健 MAD 尺度。

## Invariants

该质量量 ≠ 像素随机噪声（科学边界，不进 ivar）。

## 复杂度

O(n_ref) 单 pass + MAD 排序。

## 数值风险

log10(0/负)；MAD=0 退化 → 显式状态。

## fast/reference/oracle

合成注入偏移恢复（PHOT-001..007）。

## ID

ALG-PHOTOMETRIC-FIT-*；TEST-PHOT-*。
