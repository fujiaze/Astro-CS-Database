# AstroCS Numeric Standard

## 每个科学 double/float 必须文档化

- 单位（ADU / e- / mag / deg / arcsec / 无单位比率）；
- 坐标系（pixel / WCS RA-Dec / HEALPix NESTED / tile+local xy）；
- normalization（除以曝光、中值、median 等）；
- precision requirement（FP32/FP64 边界；默认 science=FP64）；
- valid finite domain。

## MUST

- NaN/Inf 契约：输入校验返回显式 INVALID_* 状态，禁止 NaN 传播为合法产品。
- division by zero：显式守卫或状态。
- overflow：checked 尺寸运算；科学累积用 FP64/stable sums。
- epsilon 必须说明物理/数值来源，禁止裸 `1e-6` 无来源。
- FP32/FP64 boundary：fp32 路径与 fp64 等价性测试。

## 权重/逆方差

- 权重必须正有限；ivar 优先（>0），否则 1/uncertainty² 回退（SCI-NOISE-014/015）。
- 全 0 / NaN / Inf 权重 → ZERO_VALID_WEIGHT / INVALID_INPUT。

## 关联

- docs/science/UNCERTAINTY_AND_COVARIANCE.md；
- docs/standards/CODE_STANDARD.md。
