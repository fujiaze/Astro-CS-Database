# Error Model

## 类别

CONFIG / INPUT_CORRUPT / DEPENDENCY / NUMERIC / NO_DATA / RESOURCE /
TIMEOUT / IO / SCIENCE_GATE / INTERNAL（DIAGNOSTICS_STANDARD）。

## 阶段 ID

P1.READ / P1.CALIBRATE / P1.STAR / P1.PSF / P1.PLATESOLVE /
P1.PHOTOMETRIC / P1.NOISE / P1.DRIZZLE / P1.HIPS_WRITE /
P2.COVERAGE / P2.SAMPLER / P2.UPM / P2.REJECTION / P2.INTEGRATE /
P2.HIPS_WRITE。

## 稳定错误语义

- C API：0=success；非 0=hard error（类别由调用上下文/troubleshooting 定位）。
- 可恢复科学状态经 status 字段（UNDERDETERMINED / NO_CANDIDATES /
  ALL_REJECTED / ZERO_VALID_WEIGHT / INVALID_INPUT）。
- 每个 high-risk error → troubleshooting 条目（docs/diagnostics/）。

## 契约

ERR-* 族（S2 注册，含 ERR-P2-UPM-001 畸形模型）。
