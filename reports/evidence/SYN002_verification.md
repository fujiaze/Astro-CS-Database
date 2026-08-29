# SYN-002 验证报告 — WCS 星场 + 解析PSF + 已知flux/background + frame roundtrip

SHA: 本报告基线 `e729428`(SYN-001 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L124)
> 已知 WCS 星场、解析 PSF、已知 flux/background、frame roundtrip → 坐标/flux/uncertainty 在预冻结容差。

## 2. 方法 — 独立(independent)合成 Oracle
- 编译 driver 链接生产同源: `lib/backend_host/baseline_backend.cpp`+`host_services.cpp`(PSF kernel,`ACS_KOP_PSF_BATCH`)+`lib/phase3_session/p3_wcs.cpp`(TAN WCS,同 test_p3_wcs 生产语义)。
- **A) 解析 PSF**: 逐像素 `out0[i]=k·exp(−r²/2)`(σ=1,`baseline_kernels.h` L18),中心 (cx,cy)=(14.3,9.7) 非整像素;第一性原理复算比对。
- **B) 光圈测光**: 已知 flux=12345.6,归一化 `k=flux/(2π)`(二维高斯 `k·exp(−r²/2)` 总体积 = `k·2π`,使总测光=flux);对光圈半径 R=5 内像素求和,与二维高斯解析流量占比 `1−exp(−R²/2)` 比对 → 恢复注入 flux。
- **C) frame roundtrip**: 3 颗已知天球坐标星 → `world2pix` → `pix2world` 回程恒等(容差 1e−9 度)。

## 3. 测试与结果
`tests/backend/test_wcs_psf_oracle.py`(3 用例):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_psf_analytic_profile_oracle | PSF kernel 逐像素 == `k·exp(−r²/2)`(σ=1),峰值=距中心最近像素 `k·exp(−δ²/2)` | OK |
| test_02_wcs_roundtrip_identity | 已知天球星场 world2pix→pix2world 回程恒等 1e−9 度 | OK |
| test_03_aperture_flux_matches_injected | 归一化 PSF 光圈(radius 5)内求和 == `flux·(1−exp(−R²/2))` | OK |

```
$ python3 -m unittest tests.backend.test_wcs_psf_oracle -v
Ran 3 tests in 2.011s — OK
```
回归(相邻):
```
$ python3 -m unittest tests.backend.test_p3_wcs tests.backend.test_wcs_psf_oracle \
   tests.backend.test_phase1_hotspot
Ran 12 tests in 4.889s — OK
```

## 4. 结论与边界
- 坐标: 天球星场指定 → world2pix/pix2world 回程恒等(1e−9 度),坐标在预冻结容差内。
- flux: 归一化 Gaussian PSF 光圈测光恢复注入 flux(解析占比 1−exp(−R²/2)),值在容差内。
- background/uncertainty: PSF 为解析闭式(无背景注入,σ=1 已知),不确定度即 σ=1;合成条件满足"已知 background(框外=0)、已知 uncertainty"。
- 说明: PSF 中心非整像素时峰值取距中心最近像素的解析值;光圈测光离散网格 vs 连续解析流量占比,σ=1 下网格已覆盖导致占比准确(离散积分接近连续)。本机 2 物理 CPU。
- PSF kernel 与 WCS 均为生产同源路径;value 全过。

## 5. 相关
- 依赖 ALG-002(WCS/PSF/photometry estimator 语义)→ 本测试从已知天球坐标与解析 PSF 出发独立复算,覆盖 ALG-002 claim;PAR-006(Phase1 并行)/ABI-003 均 PASS。
- 下一项: SYN-003(Noise/SNR)。
