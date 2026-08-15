# Module: dynamic_psf

## 职责

Moffat4 动态 PSF 建模与拟合质量代理（q_psf/residual_scale）。

## 非职责

不产生像素噪声权重；不进 Phase2 science weight（SNR-008 边界）。

## Public API

dynamic_psf DLL（PSF 拟合/评估）；PSF 块 [N,9] 契约。

## Data contract

星点裁剪图像 → 参数/残差列。

## Ownership

工作区 RAII；输出块调用方分配。

## Thread safety

按星点并行；拟合独立。

## Errors

拟合不收敛 → 显式状态。

## Science IDs

SCI-PSF-001；ALG-STAR-PSF-*。

## Tests

合成 Moffat 恢复（PSF-001..008）；残差 Gaussian 假设。

## Source files

lib/dynamic_psf/。
