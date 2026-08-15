# Photometry Science

## 目的

将仪器流量校准到相对/绝对光度尺度。

## 科学定义

测光定标残差（相对合成星表通量）：

```text
r_i = log10(F_instr,i / F_syn,i)      # dex
sigma_mag = 2.5 × MAD(r_inlier)/0.6745
```

PhotometricCalibrationQuality 即该残差尺度（QA/systematic metadata），
**不是**逐像素噪声方差（SCI-NOISE 边界）。

## 变量/单位

- F_instr：仪器通量（ADU·px 或 e⁻）；F_syn：合成通量（星表模型）；
- r：dex；mag：mag；sigma_cal_rel：相对误差（×ln10）。

## 假设

- 合成星表（Gaia）提供可信参考通量；大气/仪器零点在观测尺度内稳定。

## 有效域

- 亮星非饱和、PSF 解析、无云/无极端消光。

## 不保证

- 不保证颜色项之外的带通效应（QA 暴露残差分布）。

## 失效条件

- 参考星不足 → NO_DATA；残差 MAD=0 → 显式处理。

## 系统/随机误差

- 系统：零点漂移、带通不匹配；随机：星点测光噪声。

## 数值精度

FP64 对数空间；flux 比值带权（ivar 不用于此层）。

## 参考文献

Siril/IRAF 测光语义；Gaia DR3 合成光度。

## ID

SCI-PHOT-001；ALG-PHOTOMETRIC-FIT-*。
