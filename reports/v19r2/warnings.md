# Warnings Report (V19R2)

## 扫描

合并后 HEAD（PR#1）+ 本轮全部修改，全量重建：

- toolchain.ps1 build（astro_image_io/calibration/dynamic_psf/ipv/
  star_detector/snr/photometric/healpix_drizzle/gaia/orchestrator）：
  `-Wall -Wextra -Wpedantic`（模块 Makefile 已有），0 first-party warning。
- astro_image_io `make -B` 强制重建：0 warning / 0 error。
- phase2 CMake 本轮补 `-Wall -Wextra -Wpedantic` 后全量重建：
  32 条告警（cuda bridge FARPROC ×27、coverage strncpy ×1、测试 ×4）
  已全部修复（F-V19R2-BLD-001/COV-001/CUDA-001/REJ-001），当前 0 warning。
- vendored cfitsio：`-w` 编译抑制（third_party exception，V19 同策略）。

## 结论

```text
WARNING_GATE=PASS
first_party_warnings=0
```

## 证据

- reports/v19r2/evidence/quality/warnings.json
- run/temp/phase2_warn_build2.log（0 warning 构建日志）
- run/temp/aio_force_build.log（0 warning）
