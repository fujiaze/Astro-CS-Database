# phase1/noise — NoiseModel (L2 模块 README)

- 合同: `P1-005` / SCI-NOISE-001 (docs/contracts/INDEX.yaml)
- Header: `lib/phase1/noise/noise_model.h`
- Source: `lib/phase1/noise/noise_model.cpp`
- Test: `tests/unit/p1_noise_test.cpp` (6 组: blank sky/MC Poisson/低高信号/负值/gain 边界/ivar 不混)

## 职责
σ_bg = 1.4826·MAD → variance/ivar (1/variance)。floor=1e-12。
gain 诊断: variance = signal/gain + read_noise²/gain²; 零 gain/无效 read noise → 显式无效。
variance 与 ivar 显式不混。公式权威: SCI-NOISE-001。
