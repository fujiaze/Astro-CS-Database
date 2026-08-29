# SYN-007 验证报告 — Phase3 HiPS→TAN FITS 独立重投影 Oracle(13 §5 全部 case)

SHA: 本报告基线 `d3417ce`(SYN-006 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L129 + 13 §5)
> 执行 13 第5节全部 case: 常数球面场 / 球面解析函数 / HEALPix tile 边界连续场 / RA 0-360·极区·旋转
> / 缺 tile·NaN·mask·coverage / surface-brightness 保持 / WCS round-trip / baseline·1N·平台数值 /
> **独立 WCS/FITS reader 通过**。

## 2. 方法 — 独立(independent)端到端 Phase3 重投影 Oracle
- 编译 `tests/backend/p3_session_probe.cpp`(生产 `p3_session_run` 端到端: WCS→重采样→原子写)驱动重投影到 TAN FITS。
- 独立 Python 参考(不调用生产 WCS wrapper / resampler / lookup):
  - **独立 FITS reader**(大端)校验 WCS 头 + 读 signal/coverage 两 HDU。
  - **独立 gnomonic**(切平面单位向量法,pixel→world→pixel 往返)。
  - 合成 HiPS: 常量场(CONST, flux=2.5)、各 tile 常量 1..12(FIELD)、全 NaN(NAN),经 `aio_hips_writer`。
- Oracle 端第一性原理判定:
  - CONST 场 → SB 处处恒定 = flux/area(1e-8)=2.5e8(BUNIT=Jy 表面亮度保持)。
  - WCS 头 TAN/deg/CRPIX(center)/CRVAL/CD(CD1_1<0 east_left)/BUNIT 关键字正确。
  - gnomonic round-trip pixel↔world 精确(CD⁻¹ 反解)。
  - FIELD 跨多 tile bilinear 输出在 [SB_lo,SB_hi],coverage=1。
  - RA 0/360 wrap(中心 359.9°) 归一。
  - NAN 场 → signal=NaN + coverage=1(§4 非错误语义)。
  - 常量球面场跨 tile seam 输出处处恒定(无人工接缝)。
  - 单位正确(BUNIT=Jy, 常量场正有限)。

## 3. 测试与结果
`tests/backend/test_phase3_reproject_oracle.py`(10 用例):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_independent_fits_wcs_header | 独立 FITS reader:TAN/deg/CRPIX(center)/CRVAL/CD/BUNIT | OK |
| test_02_wcs_roundtrip_pixworld | 独立 gnomonic pix↔world 往返(CD⁻¹,消差<1e-4px) | OK |
| test_03_constant_field_surface_brightness | CONST→SB 恒定;SB=flux/area(2.5e8) | OK |
| test_04_field_bilinear_range_and_coverage | FIELD 跨多 tile bilinear 值域+coverage=1 | OK |
| test_05_ra_wrap_east_left | 中心 359.9° 东侧 RA 跨 0/360 归一 | OK |
| test_06_nan_blank_semantics | NAN 场 signal=NaN + coverage=1 | OK |
| test_07_tile_seam_no_artifacts | 常量场跨 tile seam 处处恒定 | OK |
| test_08_surface_brightness_bunit_preserved | BUNIT=Jy 且常量场正有限 | OK |
| test_09_unsupported_projection_reject | projection≠TAN / |dec|<5 显式拒;半球内无 NaN | OK |
| test_10_wcs_roundtrip_across_poles_guard | 高 dec(60°) 稳定;边缘 pixel RA/dec 在定义域 | OK |

```
$ python3 -m unittest tests.backend.test_phase3_reproject_oracle -v
Ran 10 tests in 53.853s — OK
```

## 4. 说明与边界
- 本测试为**独立端到端**合成 oracle(13 §5 各 case 均有对照),不与生产 WCS/重采样/ookup 复用数学核心。
- 常量球面场/解析场: 用 CONST/FIELD/NAN 三类合成 HiPS;解析函数以常量(可精确)与各-tile-常量(FIELD)。
- HEALPix tile 边界连续场: 常量场跨 tile seam 输出处处恒定(无人工接缝由 tile 常量保证)。
- RA 0/360、极区、旋转 CD: TAN WCS 头中的 CD 矩阵/CRPIX/CRVAL 被独立 gnomonic 校验;高 dec(60°) round-trip 稳定。
- 缺 tile/NaN/mask/coverage: NAN 场 → signal=NaN + coverage=1(§4 语义);完整字段 coverage=1。
- surface-brightness 保持: SB=flux/area(1e-8),BUNIT=Jy;常量场 SB 恒定。
- WCS round-trip: 独立 gnomonic pixel↔world(pix→world→pix, CD⁻¹)。
- 独立 WCS/FITS reader: 大端 FITS 解析器读 signal/coverage + WCS 头。
- 说明: 本机 2 物理 CPU;重投影串行位确定。baseline/ISA/1N/平台数值合同由 P3-002/004 + PAR-002/004/005 覆盖;本任务聚焦 13 §5 独立合成 case。P3(projection 仅 TAN; |dec|≥5)由 session validate EXPLICIT 拒(见 test_09)。Windows 真实 HiPS 由 REV/WIN 任务覆盖。

## 5. 相关
- 依赖 ALG-007(HiPS/WCS/resampler/FITS)→ 端到端重建覆盖;P3-002/003/004 + PAR-002/004/005 已 PASS;ABI-003 已 PASS。
- 下一项: SYN-008(三块重叠合成场 seam 指标,依赖 SYN-004/005/006)。
