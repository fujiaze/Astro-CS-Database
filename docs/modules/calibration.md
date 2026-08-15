# Module: calibration

## 职责

masterBias/Dark/Flat 生成、图像校准（bias/dark/flat）、坏点修复。

## 非职责

不做天体测量/测光/噪声模型。

## Production callers

orchestrator stage1（CALIBRATE）；Python 调试层（非生产）。

## Public API

astro_calibration.h（AC_API extern "C"）；cosmetic_corrector。

## Data contract

FITS 输入输出；母版分组（曝光/滤镜）。

## Ownership

句柄级；输出 buffer 调用方提供。

## Thread safety

OpenMP parallel-for 像素块；母版只读。

## Errors

母版缺失/不匹配 → CONFIG/NO_DATA；除零 → NUMERIC。

## Config

母版路径/曝光/滤镜；无全局状态。

## Science IDs

SCI-CAL-001；ALG-CAL-001..。

## 性能特征

O(pixels) 单 pass；16 线程块并行。

## 缓存

母版在 orchestration 层缓存。

## Diagnostics

每帧日志：曝光/滤镜/母版 hash。

## Tests

合成主帧/噪声注入（CAL-001..006）；Python 对照。

## Source files

lib/calibration/{include,src}/。
