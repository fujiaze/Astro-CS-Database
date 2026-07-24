# 代码审计 checklist - 2026-07-18

配套 spec：`docs/superpowers/specs/2026-07-18-code-audit.md`

## A. 准备（A1-A2）
- [ ] A1. 读取 4 个对照文档关键章节（PROJECT_OVERVIEW/ARCHITECTURE/PIPELINE_OVERVIEW/DESIGN_IMPL_GAP）
- [ ] A2. 读取 project_memory.md 硬约束清单

## B. 9 模块并行扫描（B1-B9，每模块独立 checklist）

### B1. astro_image_io 扫描
- [ ] B1.1 扫描 `src/aio_api.cpp`（对外 C API）
- [ ] B1.2 扫描 `src/aio_pipeline.cpp` + `aio_pipeline_engine.cpp`（PipelineFrame 命名块）
- [ ] B1.3 扫描 `src/aio_fits.cpp` + `aio_xisf.cpp` + `aio_compressor.cpp`（I/O + 压缩）
- [ ] B1.4 扫描 `src/healpix/aio_healpix_io.cpp`（.hiss/.hcsd 读写）
- [ ] B1.5 扫描 `include/*.h`（API 契约）
- [ ] B1.6 对照文档检查 4 个方面（stage handler / 算法 / 数据格式 / 运维）
- [ ] B1.7 输出问题清单（按 Critical/High/Medium/Low 分级）

### B2. calibration 扫描
- [ ] B2.1 扫描 `src/ac_api.cpp`（对外 C API）
- [ ] B2.2 扫描 `src/calibrator.cpp`（dark/flat/bias 校准）
- [ ] B2.3 扫描 `src/cosmetic_corrector.cpp` + `master_generator.cpp`
- [ ] B2.4 扫描 `include/astro_calibration.h`
- [ ] B2.5 对照文档检查 4 个方面
- [ ] B2.6 输出问题清单

### B3. plate_solve 扫描
- [ ] B3.1 扫描 `cpp/ipv/src/ipv_entry.cpp` + `ipv_solver.cpp`（对外 API + 主流程）
- [ ] B3.2 扫描 `ipv_ransac.cpp` + `ipv_robust_refine.cpp`（RANSAC + Umeyama SVD）
- [ ] B3.3 扫描 `ipv_kvector.cpp` + `ipv_triangle.cpp` + `ipv_polygon.cpp`（向量匹配）
- [ ] B3.4 扫描 `ipv_select.cpp` + `ipv_score.cpp` + `ipv_verify.cpp`（选择/评分/验证）
- [ ] B3.5 扫描 `ipv_wcs.cpp` + `ipv_sip.cpp` + `ipv_distortion.cpp` + `ipv_itertrans.cpp` + `ipv_angle.cpp`
- [ ] B3.6 扫描 `include/ipv_*.h`（API 契约）
- [ ] B3.7 对照文档检查 4 个方面（重点：gnomonic 投影 / s 限制 ±10% / 动态 inlier 阈值 / 5 次 MAD）
- [ ] B3.8 输出问题清单

### B4. dynamic_psf 扫描
- [ ] B4.1 扫描 `src/dpsf_psf.cpp`（Moffat4 PSF 拟合核心）
- [ ] B4.2 扫描 `src/dpsf_image.cpp`（图像处理）
- [ ] B4.3 扫描 `src/dpsf_log.cpp`（日志）
- [ ] B4.4 扫描 `include/dynamic_psf.h`
- [ ] B4.5 对照文档检查 4 个方面
- [ ] B4.6 输出问题清单

### B5. photometric_calib 扫描
- [ ] B5.1 扫描 `cpp/src/pc_api.cpp`（对外 C API + scale 求解 + **应用 scale 到图像**）
- [ ] B5.2 扫描 `cpp/src/spectrum_integrator.cpp`（F_syn 积分）
- [ ] B5.3 扫描 `cpp/src/star_matcher.cpp`（PSF 流量 vs Gaia 积分流量匹配）
- [ ] B5.4 扫描 `cpp/src/image_corrector.cpp` + `wcs_transform.cpp`
- [ ] B5.5 扫描 `cpp/include/photometric_calib.h` + `log_macros.h`
- [ ] B5.6 对照文档检查 4 个方面（重点：IRLS+Tukey / 应用 scale / photo_stats 输出）
- [ ] B5.7 输出问题清单

### B6. snr_estimator 扫描
- [ ] B6.1 扫描 `cpp/src/snr_estimator.cpp`（乘法模型 SNR = SNR_phot × (IDW/median)）
- [ ] B6.2 扫描 `cpp/include/snr_estimator.h`
- [ ] B6.3 对照文档检查 4 个方面（重点：乘法模型 / 控制点 / IDW 球面）
- [ ] B6.4 输出问题清单

### B7. healpix_drizzle 扫描
- [ ] B7.1 扫描 `hp_drizzle_api.cpp`（对外 C API）
- [ ] B7.2 扫描 `drizzle_engine.cpp`（Drizzle 核心）
- [ ] B7.3 扫描 `wcs_sip.cpp` + `poly_clip.cpp`（WCS+SIP + 多边形裁剪）
- [ ] B7.4 扫描 `fits_reader.cpp`（输入读取）
- [ ] B7.5 扫描对应 `.h`
- [ ] B7.6 对照文档检查 4 个方面（重点：CD 矩阵无 1/cos(Dec) / nside 自适应 / .hiss 输出）
- [ ] B7.7 输出问题清单

### B8. healpix_stack 扫描
- [ ] B8.1 扫描 `hp_stack_api.cpp` + `hp_stack_hiss.cpp`（对外 C API + .hiss 输入）
- [ ] B8.2 扫描 `stack_engine.cpp` + `stack_db.cpp`（堆叠 + .hcsd 输出）
- [ ] B8.3 扫描 `ahps_reader.cpp` + `ahps_writer.cpp` + `healpix_core.cpp`
- [ ] B8.4 扫描 `gradient/corrected_stacker.cpp`（球面梯度校正 + SNR² 加权叠加）
- [ ] B8.5 扫描 `gradient/gradient_fitter.cpp` + `gradient_sampler.cpp` + `spherical_spline.cpp`
- [ ] B8.6 扫描 `gradient/snr_evaluator.cpp`（SNR 评估）
- [ ] B8.7 扫描对应 `.h`
- [ ] B8.8 对照文档检查 4 个方面（重点：Winsorized sigma clip / SNR² 加权 / .hcsd 子叶块索引）
- [ ] B8.9 输出问题清单

### B9. orchestrator 扫描
- [ ] B9.1 扫描 `cpp/src/orchestrator.cpp` 中 7 个 stage1 handler + 2 个 stage2 handler
- [ ] B9.2 扫描 `cpp/src/orchestrator.cpp` 中 init_dlls / init_platesolve_env / cleanup
- [ ] B9.3 扫描 `cpp/src/dll_loader.cpp`（9 模块加载）
- [ ] B9.4 扫描 `cpp/src/cli_command.cpp` + `cli_repl.cpp` + `main.cpp` + `checkpoint.cpp` + `logger.cpp`
- [ ] B9.5 扫描 `cpp/include/*.h`
- [ ] B9.6 对照文档检查 4 个方面（重点：stage handler 参数传递 / 命名块契约 / 错误处理 / 资源生命周期）
- [ ] B9.7 输出问题清单

## C. 汇总（C1-C3）
- [ ] C1. 汇总 9 份问题清单 → 总报告
- [ ] C2. 按严重度排序，统计表（Critical/High/Medium/Low 各多少）
- [ ] C3. 提交用户审阅，等待修复范围决策

## D. 修复决策（D1-D3，由用户决定后执行）
- [ ] D1. 用户审阅总报告
- [ ] D2. 用户决定修复范围（Critical+High / +Medium / 全修）
- [ ] D3. 出修复 spec + checklist → 执行 → 验证
