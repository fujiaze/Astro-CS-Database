# Module: photometric_calib

## 职责

测光定标（合成星表相对流量）→ PhotometricCalibrationQuality。

## 非职责

不产生逐像素 ivar（边界见 docs/science/PHOTOMETRY.md）。

## Public API

photometric_calib DLL（flux_calibrator）。

## Data contract

仪器/合成流量 → dex 残差 + sigma_mag/sigma_cal_rel。

## Ownership

输出结构调用方释放。

## Thread safety

帧级串行；无共享状态。

## Errors

参考星不足 → NO_DATA。

## Science IDs

SCI-PHOT-001；ALG-PHOTOMETRIC-FIT-*。

## Tests

注入偏移恢复（PHOT-001..007）。

## Source files

lib/photometric_calib/。
