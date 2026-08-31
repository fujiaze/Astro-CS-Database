# P1-002: 生产 Oracle

任务 ID: P1-002
Gate: G4
依赖: P1-001
平台: Linux
变更类别: validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P1-002：

> 所有测试通过 Registry/Runtime 或正式 CLI 执行；Oracle 独立实现，不调用待测 kernel；
> seed、容差、单位预冻结。
> Calibration/stars-PSF/WCS/photometry/noise/Drizzle 全类别覆盖。

## 验收项与实现对照

| 类别 | 覆盖 | 证据 |
|---|---|---|
| Calibration | 常量/梯度/负值/NaN、bias/dark/flat、u16/f32/f64、跨线程确定性 | c02_calibration |
| stars/PSF | 五场景(孤立/重叠/饱和/边缘/纯噪声); completeness=1.0/FP=0/centroid<0.5px/FWHM≈2.3548σ | c01_gaps |
| WCS | 已知场(解析解 1e-9px)/扰动初值(0.5px 收敛)/roundtrip(1e-9 度)/无解(PARAM/HEMISPHERE) | c01_gaps |
| photometry | 已知 flux/background/PSF 光圈恢复(2%); flux 线性; 饱和拒绝(sat 星不进测光) | c01_gaps |
| noise | 高斯/边界/离群/Poisson+read noise 解析 variance/尺度律/空间填充 | c02_noise |
| Drizzle | overlap/support/accumulate/normalize/flux 守恒/亚像素/RA wrap | c02_drizzle |

## 实现文件

- `tests/backend/test_p1002_gaps.py`（新，subagent 交付）：stars/PSF 五场景 + WCS 三态 + photometry 饱和拒绝
  独立合成 Oracle（GSL trust-region LM 生产符号 sdet_*、p3_wcs、photometer）
- ABI 遗留修复（CPU-001 head 引入后测试同步）：
  `test_drizzle_oracle.py`/`test_wcs_psf_oracle.py`/`test_drizzle_parallel.py`/`test_phase1_hotspot.py`：
  span brace-init → ACS_SPAN_F32/U8; `p3_session_probe.cpp` 同; `test_phase3_reproject_oracle.py`：
  +version_generated.h include

## 测试结果

- 6 个 Oracle 测试全绿：test_calibration_oracle / test_drizzle_oracle / test_noise_model_oracle /
  test_wcs_psf_oracle / test_phase3_reproject_oracle / test_p1002_gaps(14 tests)
- 其它 backend 测试（isa/abi/bench/hardware/hips）全绿

## 说明

- 环境：subagent 系统级安装 libgsl-dev(2.8) 以满足 sdet_* 生产符号链接（GSL 依赖）。
- seed/容差全部预冻结写死在测试中（FWHM 2.3548σ 8%、centroid 0.5px、flux 2% 等）。
