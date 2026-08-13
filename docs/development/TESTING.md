# Testing

## 测试入口

- `phase2_synthetic_gate.exe`（Phase2 合成门，V2–V13 逐轮累积）。
- `lib/astro_image_io/tests/*`（tile mapping oracle、sanitize）。
- `v13_synth_test.exe`（background-clean sampler 真值：多星/多 PSF/多亮度
  recall/false-reject/connectivity）。
- `toolchain.ps1 check|build` 做环境与编译验证。

## 关键回归集

- HiPS 序列化：Hipsgen 外部 oracle（205625/205627）。
- Phase2 sampler：无星梯度保留 ≥95%、星污染拒绝 recall ≥95%、false
  reject ≤5%、dense connectivity。
- UPM：V13 science 逐位等价（C/M maxdiff=0）作为回归锚。
- Cross-stage contract：Phase1 输出（signal/support/MOC/SNR/quality/
  frame_id/manifest）被 Phase2 与外部标准一致读取。
- Browser：V13 产品几何显示不回归。

## 运行

```powershell
py -3.12 lib/phase2/build/phase2_synthetic_gate.exe
.\toolchain.ps1 check
```
