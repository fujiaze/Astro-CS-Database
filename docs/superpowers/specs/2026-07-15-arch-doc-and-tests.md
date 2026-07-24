# 架构文档更新 + Drizzle/Photometric 测试补全

**日期**: 2026-07-15
**类型**: 功能实现（文档 + 测试）
**状态**: 待执行

---

## 1. 背景

截至 2026-07-15，项目已完成 Qt6 浏览器重构、Drizzle 黑色缝隙修复、浏览器性能/视觉优化、双击启动部署等工作，但：

- `PROJECT_ARCHITECTURE.md` 日期停留在 2026-07-13，缺失后续工作记录
- `test_drizzle.py` 仍引用已废弃的 `ahpx_io` 和 `.ahpx` 格式
- `photometric_calib` 缺独立的能量守恒测试

本次任务需补全上述三项。

---

## 2. 目标

### 2.1 架构文档重写

覆盖更新 `PROJECT_ARCHITECTURE.md`：

- 文档日期：2026-07-13 → 2026-07-15
- §2.1 模块清单：`healpix_browser_qt/` 标为活跃；旧 `healpix_browser_cpp/` + `healpix_browser_web/` 标为已归档（移至 `archive/`）
- §10 浏览器架构：补 Qt6 + OpenGL 3.3 Core 三层架构细节
  - LOD 动态 `nside_target`（FOV 自适应：FOV=15.73° → nside=4096，HEALPix 像素 0.0143° < 屏幕像素 0.0211°）
  - uint8 降采样缓存（节省 75% 内存）
  - 菱形像素 mesh（替代矩形近似，消除 cos_dec 发散）
  - `MAX_SHIFT=4` 递减查找（避免远处像素填充无数据区域）
  - `deploy.ps1` 双击启动（22 个 MinGW runtime + Qt6 间接依赖 DLL）
  - `auto_stretch` 0.5%/99.5% 分位（替代 median±3sigma，只统计有数据像素）
- §6.2/6.3 进度：Qt6 浏览器、Drizzle 缝隙修复、deploy.ps1 标为已完成
- §11 性能优化记录：新增 §11.4 Drizzle 黑色缝隙修复 + §11.5 浏览器性能/视觉修复

### 2.2 Drizzle 测试重写

完全重写 `lib/healpix_db/healpix_drizzle/tests/test_drizzle.py`，7 项测试：

| # | 测试名 | 类型 | 数据 | 验证点 |
|---|--------|------|------|--------|
| 1 | `test_wcs_tan_roundtrip` | 合成 | astropy WCS | CRPIX→CRVAL + 多点 pixel→sky→pixel 往返 |
| 2 | `test_pixfrac0_point_sampling` | 合成 | 10×10 均匀 | n_source_pixels + 点采样亮度 = 输入值 |
| 3 | `test_pixfrac1_area_allocation` | 合成 | 10×10 均匀 | 通量守恒 + 面积分配亮度 = 输入值 |
| 4 | `test_hiss_roundtrip` | 合成 | 50×50 均匀 | Drizzle→HissReader 往返：ipix/pixel/meta 一致 |
| 5 | `test_sip_distortion` | 合成 | 100×100 + SIP A/B | 带 SIP 畸变 FITS→HEALPix，像素落点正确（与 astropy+WCS 对比） |
| 6 | `test_gradient_flux_conservation` | 合成 | 100×100 梯度 | 非均匀亮度通量守恒（亮度 = sum(value×w)/sum(w)） |
| 7 | `test_real_fits_endtoend` | 真实 | `testdata/` 一帧 | n_source_pixels / n_healpix_pixels 合理，无零值像素 |

### 2.3 photometric 能量守恒测试

新建 `lib/photometric_calib/cpp/test/test_energy_conservation.py`，4 项测试：

| # | 测试名 | 验证点 |
|---|--------|--------|
| 1 | `test_pixel_level_conservation` | 均匀图像 I → I×scale，每像素 = I_input × scale（atol=1e-5） |
| 2 | `test_matched_star_conservation` | 合成 PSF + Gaia（F_instr = F_syn/10），验证 scale ≈ 10.0 + F_cal = F_instr×scale ≈ F_syn |
| 3 | `test_residual_distribution` | N 颗匹配星 F_cal/F_syn 中位数 ≈ 1.0，MAD < 5% |
| 4 | `test_degenerate_paths` | n_psf=0 / n_gaia=0 / 匹配距离 > 3px 均正确处理（错误码或不产生假匹配） |

### 2.4 测试数据策略

- **合成小图**：100×100 / 200×200，astropy WCS 构造，精确可控，验证逻辑正确性
- **真实数据**：`testdata/` 一帧 FITS，验证端到端管线行为

---

## 3. 约束

- Drizzle 测试默认 `pixfrac=1.0`（项目当前默认值）
- 所有测试 DLL 不可用时 `pytestmark.skipif` 跳过（不 fail）
- 测试日志输出到 `%TEMP%\test_<module>.log`
- 测试不依赖网络（photometric 能量守恒用合成数据，不调用 DR3SP）
- 测试代码开头写清功能、用途（用户规则）

---

## 4. 验收标准

### 4.1 架构文档

- [ ] 日期更新为 2026-07-15
- [ ] 包含 Qt6 浏览器重构章节（三层架构 + 关键决策）
- [ ] 包含 Drizzle 黑色缝隙修复章节
- [ ] 包含浏览器性能/视觉修复章节
- [ ] 模块清单状态正确（active/archived/deprecated）
- [ ] 进度表反映 2026-07-14/15 完成的工作

### 4.2 Drizzle 测试

- [ ] 7 项测试全部通过（或 DLL 不可用时正确跳过）
- [ ] 不再引用 `ahpx_io` / `.ahpx`
- [ ] 使用 `HissReader` 读取 `.hiss`
- [ ] 真实数据测试需 `testdata/` 中存在 FITS 文件，否则跳过
- [ ] 测试日志输出可读

### 4.3 photometric 能量守恒测试

- [ ] 4 项测试全部通过
- [ ] 不依赖 DR3SP 网络
- [ ] 退化路径测试覆盖 n_psf=0 / n_gaia=0 / 匹配距离过大三种情况
- [ ] 测试日志输出可读

---

## 5. 实现路径

```
1. 重写 PROJECT_ARCHITECTURE.md（覆盖更新）
2. 重写 lib/healpix_db/healpix_drizzle/tests/test_drizzle.py
3. 新建 lib/photometric_calib/cpp/test/test_energy_conservation.py
4. 运行 Drizzle 测试套件（pytest）
5. 运行 photometric 能量守恒测试（pytest）
6. 更新 memory.md（根 memory + 模块 memory）
7. 报告 + 追问下一阶段
```

---

## 6. 风险

- 真实数据测试依赖 `testdata/` 中 FITS 文件存在；若不存在需 skip 而非 fail
- photometric 测试需 DLL 已编译（`photometric_calib.dll`）
- SIP 测试需 astropy 生成参考 RA/Dec，与 C++ WcsSip 实现对比（允许亚像素误差）
