# 任务分解

**关联 spec**: `2026-07-15-arch-doc-and-tests.md`

---

## 阶段 1: 架构文档重写

| # | 任务 | 文件 | 依赖 |
|---|------|------|------|
| T1.1 | 更新文档头部日期 | `PROJECT_ARCHITECTURE.md` | — |
| T1.2 | §2.1 模块清单状态更新 | `PROJECT_ARCHITECTURE.md` | T1.1 |
| T1.3 | §2.2/2.3 依赖图更新 | `PROJECT_ARCHITECTURE.md` | T1.2 |
| T1.4 | §10 浏览器架构补 Qt6 细节 | `PROJECT_ARCHITECTURE.md` | T1.3 |
| T1.5 | §6.2/6.3 进度表更新 | `PROJECT_ARCHITECTURE.md` | T1.4 |
| T1.6 | 新增 §11.4 Drizzle 缝隙修复 | `PROJECT_ARCHITECTURE.md` | T1.5 |
| T1.7 | 新增 §11.5 浏览器性能修复 | `PROJECT_ARCHITECTURE.md` | T1.6 |

## 阶段 2: Drizzle 测试重写

| # | 任务 | 文件 | 依赖 |
|---|------|------|------|
| T2.1 | 改写文件头注释 + 移除 ahpx_io 依赖 | `tests/test_drizzle.py` | T1.7 |
| T2.2 | 改写 create_test_fits（保留）+ 新增 create_sip_fits | 同上 | T2.1 |
| T2.3 | 测试 1: test_wcs_tan_roundtrip（保留，纯 astropy） | 同上 | T2.2 |
| T2.4 | 测试 2: test_pixfrac0_point_sampling（改 HissReader） | 同上 | T2.3 |
| T2.5 | 测试 3: test_pixfrac1_area_allocation（pixfrac=1.0 + HissReader） | 同上 | T2.4 |
| T2.6 | 测试 4: test_hiss_roundtrip（Drizzle→HissReader） | 同上 | T2.5 |
| T2.7 | 测试 5: test_sip_distortion（SIP A/B 合成 + astropy 对比） | 同上 | T2.6 |
| T2.8 | 测试 6: test_gradient_flux_conservation（梯度图像） | 同上 | T2.7 |
| T2.9 | 测试 7: test_real_fits_endtoend（testdata 真实 FITS） | 同上 | T2.8 |

## 阶段 3: photometric 能量守恒测试

| # | 任务 | 文件 | 依赖 |
|---|------|------|------|
| T3.1 | 新建文件头 + 路径设置 + DLL 加载检测 | `cpp/test/test_energy_conservation.py` | T2.9 |
| T3.2 | 测试 1: test_pixel_level_conservation | 同上 | T3.1 |
| T3.3 | 测试 2: test_matched_star_conservation | 同上 | T3.2 |
| T3.4 | 测试 3: test_residual_distribution | 同上 | T3.3 |
| T3.5 | 测试 4: test_degenerate_paths | 同上 | T3.4 |

## 阶段 4: 运行测试

| # | 任务 | 命令 | 依赖 |
|---|------|------|------|
| T4.1 | 运行 Drizzle 测试 | `python -m pytest tests/test_drizzle.py -v -s` | T2.9 |
| T4.2 | 运行 photometric 测试 | `python -m pytest test/test_energy_conservation.py -v -s` | T3.5 |
| T4.3 | 修复失败用例（如有） | — | T4.1/T4.2 |

## 阶段 5: 收尾

| # | 任务 | 文件 | 依赖 |
|---|------|------|------|
| T5.1 | 更新根 memory.md | `memory.md` | T4.3 |
| T5.2 | 更新 drizzle 模块 memory | `lib/healpix_db/healpix_drizzle/memory.md` | T5.1 |
| T5.3 | 更新 photometric 模块 memory | `lib/photometric_calib/memory.md` | T5.2 |
| T5.4 | 报告 + 追问下一阶段 | — | T5.3 |
