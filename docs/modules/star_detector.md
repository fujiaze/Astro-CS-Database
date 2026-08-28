# Module: star_detector

## 职责

星点检测/去重/质心（为 PSF 与 plate solve 提供输入）。

## 非职责

不做匹配/求解。

## Public API

sdet_api（extern "C"）。

## Data contract

图像 + 阈值 → 星表（x,y,flux）。

## Ownership

输出表调用方释放。

## Thread safety

OpenMP 块并行；去重后处理串行。

## Errors

无星点 → NO_DATA。

## Science IDs

SCI-PSF-001（输入侧）；ALG-STARPSF-*。

## Tests

合成星场检测率/虚警。

## Source files

lib/star_detector/。
