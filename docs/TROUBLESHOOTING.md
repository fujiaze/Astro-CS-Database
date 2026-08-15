# AstroCS 排障手册 (V19)

## Symptom → 定位 → 修复

| 症状 | 阶段/证据 | 可能原因 | 命令/修复 |
|---|---|---|---|
| orchestrator 启动报 DLL 加载失败 | E100 + stderr | mingw64 不在 PATH; DLL 依赖缺失 | `$env:Path="C:\msys64\mingw64\bin;$env:Path"; .\toolchain.ps1 check` |
| `psf 块不存在` | E200 + SNR 日志 | PSF 阶段未运行/失败 | 检查 stop_after; 重跑 PSF |
| 校准后全 0 / 无变化 | CALIBRATE 日志 | master 尺寸/类型不匹配 | 核对 testdata calibration files |
| platesolve RMS 异常大 | PLATESOLVE 日志 | OBJCTRA/DEC 初值错; SIP order 高 | 修正 header; 降 order |
| SNR 阶段 `NOISE_MODEL_STATUS=SKIPPED_*` | SNR 日志 | data 块缺失/API 缺失 | 确认 SNR DLL 为新版 (V19) |
| Drizzle 输出全 NaN | DRIZZLE 日志 | WCS/SIP 病态 | 检查 CRVAL/CD/A/B 系数; pixfrac |
| HiPS verify 失败 | HIPS_VERIFY 日志 | tile 布局/产品缺失 | 用 aio_hips_reader 复读; 检查 variance/ivar |
| stage2 `ZERO_VALID_WEIGHT` | INTEGRATE 日志 | 帧无 ivar 产品且 support=0 | 确认 Phase1 输出了 variance/ivar; 升级帧 |
| stage2 `INVALID_CONFIGURATION` | CONFIG 日志 | rejection normalization 组合非法 | percentile 必须 median_center; rcr 必须 none |
| 权重全等权 (模式 ivar 失效) | diag `ivar_product_missing>0` | 输入帧无 ivar 产品 (V19 前产物) | 重跑 Phase1 V19 链; 或接受 support 回退 |
| 性能下降/超时 | E980 + `operation_counts.json` | 候选效率异常 / 线程配置 | 检查 candidate_efficiency; threads 配置 |

## 常用命令

```powershell
# 环境自检
.\toolchain.ps1 check
# 全量构建
.\toolchain.ps1 build
# SNR 科学矩阵 (模块级)
lib\snr_estimator\cpp\test\noise_model_science_test.exe
# Drizzle 方差传播科学测试
lib\healpix_db\healpix_drizzle\tests\variance_propagation_test.exe
# Phase2 合成 gate
lib\phase2\build\phase2_synthetic_gate.exe
# 诊断
py -3.12 tools\astrocs_diagnose.py run\logs --json diag.json
```

## 外部依赖与网络

- GaiaDR3/GaiaDR3SP: 本地只读目录, 禁止删除
- 所有外部进程/网络等待必须带 timeout
- 不跑 BASS 大数据 (V20 才做)
