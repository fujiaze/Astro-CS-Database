# snr_estimator - 模块开发memory

## 模块职责
SNR 估算模块,基于乘法模型计算每像素信噪比:SNR = SNR_phot × (SNR_psf/median)

## 当前版本
- 版本号：v1.0 C++ DLL
- 创建时间：2026-07-15

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Snr-Estimator-C-Python-
- 默认分支：main

## 依赖列表
- C++17, OpenMP
- Python ctypes

## 关键决策记录
- 乘法模型: SNR = SNR_phot × (SNR_psf/median)
- IDW 插值: power=2.0, 搜索半径=FOV对角线
- 退化路径: n_stars=0 全填 SNR_phot, sigma_residual=0 全填 1.0

## 进度日志
### 2026-07-15 v1.0 创建
- 新建 snr_estimator 模块 (spec: snr-module-and-fault-fixes)
- C++ snr_estimate API 实现
- Python ctypes 封装
- 6 项模块自测

## 2026-08-15 V19 三层科学重构 (f672777)
- 新增 noise_model.cpp: PhotometricCalibrationQuality (dex/mag 单位正确化) /
  PsfFitQuality (q_psf=A/residual_scale, legacy (A-B)/mad 退休) /
  NoiseWeightModelV1 (source-masked blank-sky robust variance -> ivar,
  空间平面场, gain/read-noise 交叉验证, 经验 fallback)
- 科学矩阵 noise_model_science_test 32/32 (SNR-001..010/013/014 + 单位/语义)
- legacy snr_extract_model_* 保留为 diagnostic/migration (LEGACY_SNR_SCIENCE_CONSUMER=0)
