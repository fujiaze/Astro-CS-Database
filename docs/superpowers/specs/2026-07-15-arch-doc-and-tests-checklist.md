# 验收清单

**关联 spec**: `2026-07-15-arch-doc-and-tests.md`

---

## A. 架构文档

- [ ] A1 文档头部日期更新为 2026-07-15
- [ ] A2 §2.1 模块清单：`healpix_browser_qt/` 标为活跃；`healpix_browser_cpp/` + `healpix_browser_web/` 标为已归档
- [ ] A3 §2.2/2.3 依赖图：Qt6 浏览器替代旧架构
- [ ] A4 §10 浏览器架构：补 LOD 动态 nside_target + uint8 降采样 + 菱形像素 mesh + MAX_SHIFT=4 + deploy.ps1 + auto_stretch 分位
- [ ] A5 §6.2 进度表：Qt6 浏览器、Drizzle 缝隙修复、deploy.ps1 标为已完成
- [ ] A6 §6.3 未开始：移除已完成项
- [ ] A7 新增 §11.4 Drizzle 黑色缝隙修复（5基准全1-ring + 菱形像素 + pixfrac=1.0）
- [ ] A8 新增 §11.5 浏览器性能/视觉修复（FOV 自适应 / 缓存 nside 检查 / 视口物理像素 / 递减查找 / auto_stretch）

## B. Drizzle 测试

- [ ] B1 test_wcs_tan_roundtrip 通过
- [ ] B2 test_pixfrac0_point_sampling 通过
- [ ] B3 test_pixfrac1_area_allocation 通过
- [ ] B4 test_hiss_roundtrip 通过
- [ ] B5 test_sip_distortion 通过
- [ ] B6 test_gradient_flux_conservation 通过
- [ ] B7 test_real_fits_endtoend 通过（或 testdata 无 FITS 时正确 skip）
- [ ] B8 不再引用 ahpx_io / .ahpx
- [ ] B9 使用 HissReader 读取 .hiss

## C. photometric 能量守恒测试

- [ ] C1 test_pixel_level_conservation 通过
- [ ] C2 test_matched_star_conservation 通过
- [ ] C3 test_residual_distribution 通过
- [ ] C4 test_degenerate_paths 通过
- [ ] C5 不依赖 DR3SP 网络
- [ ] C6 退化路径覆盖 n_psf=0 / n_gaia=0 / 匹配距离过大三种

## D. 收尾

- [ ] D1 更新根 memory.md
- [ ] D2 更新 lib/healpix_db/healpix_drizzle/memory.md
- [ ] D3 更新 lib/photometric_calib/memory.md（若存在）
- [ ] D4 测试日志输出到 %TEMP%\test_*.log
