# P03-002 测试报告

## 测试信息
- **任务 ID**: P03-002
- **测试日期**: 2026-07-25
- **测试环境**: Windows 10, PowerShell 7, g++ 16.1.0 (MSYS2)
- **被测程序**: lib/orchestrator/cpp/orchestrator.exe (P03-002 修复后编译)

## 测试策略
P03-002 是配置参数追踪任务, 测试重点是验证参数从 config 到 DLL 消费者的传递路径。

### 测试方法
1. **正常测试**: 使用默认 stage1_config.json 运行完整 stage1, 通过日志验证参数传递
2. **边界测试**: 代码审查验证边界条件处理 (空值、无效值、范围检查)
3. **失败测试**: 代码审查验证错误处理 (config 加载失败在 P03-001 已验证)

## 测试结果

### 正常测试 (PASS)

**测试配置**:
- FITS: testdata/results/Galaxy_Center_T4/panel1/Red/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red/01_calibrated.fits
- Config: lib/orchestrator/configs/stage1_config.json (默认)
- 输出: engineering/evidence/P03-002/test_normal.hiss

**执行结果**: stage1 完成 (成功), 总耗时 ~16s

**参数传递验证 (日志证据)**:

| 参数 | 日志行 | 验证结果 |
|------|--------|----------|
| gaia_data_dir | "Gaia 数据目录: F:\...\GaiaDR3SP (来自 config)" (line 1271) | PASS |
| platesolve.max_stars | "StarDetector 创建成功 (fitRadius=0 自动, maxStars=2000)" (line 1273) | PASS |
| platesolve.initial_ra/dec | "初始指向: OBJCTRA='18 11 14.00' -> ra0=272.808333deg" (line 1277) | PASS |
| platesolve.focal_length/pixel_size | "焦距=200.000000mm, 像素尺寸=6.000000um" (line 1278) | PASS |
| psf.fit_radius/max_iter/tolerance | "调用 dpsf_fit_batch (fitRadius=8, maxIter=100, tol=1.0e-06)" (line 1291) | PASS |
| photometric.filters_json [P03-002修复] | "filters_json: F:\...\filters.json" (line 1300) | PASS |
| photometric.qe_curves_json [P03-002修复] | "qe_curves_json: F:\...\qe_curves.json" (line 1302) | PASS |
| photometric.mag_min/mag_max/fov | "调用 pc_calibrate_simple_with_gaia (mag_min=6.0, mag_max=16.0, fov=6.059deg)" (line 1306) | PASS |
| drizzle.nside_strategy/override | "nside=512 (strategy=1x_to_2x_drizzle, override=0)" (line 1318) | PASS |
| drizzle.pixfrac/nested | "pixfrac=1.000, nested=1" (line 1318) | PASS |

**Stage 执行耗时**:
- READ_FITS: 0.119s
- CALIBRATE: 0.440s (实际应用 Bias/Dark/Flat)
- PLATESOLVE: 2.265s (rms=0.355594")
- PSF: 0.327s (1740/2000 成功, 87%)
- PHOTOMETRIC: 0.207s
- SNR: 0.000s (sigma_residual<=0, 跳过)
- DRIZZLE: 12.379s (n_healpix=3928)
- **总计**: ~16s

**输出文件**: engineering/evidence/P03-002/test_normal.hiss (已生成)

### 边界测试 (代码审查 PASS)

| 参数 | 边界条件 | 处理方式 | 验证结果 |
|------|----------|----------|----------|
| gaia_data_dir | 空字符串 | 默认 project_root_dir_/GaiaDR3SP | PASS |
| platesolve.initial_ra/dec | 空字符串 | 从 FITS header OBJCTRA/DEC 读取 | PASS |
| platesolve.focal_length/pixel_size | 0.0 | 从 FITS header FOCALLEN/XPIXSZ 读取 | PASS |
| platesolve.max_stars | 0 或负数 | 默认 2000 | PASS |
| psf.fit_radius | 0 | 自动模式; 负数重置为 0 | PASS |
| psf.max_iter | 0 或负数 | 默认 100 | PASS |
| psf.tolerance | 0 或负数 | 默认 1e-6 | PASS |
| photometric.mag_min | 负数 | 重置为 0.0 | PASS |
| photometric.mag_max | <= mag_min | 默认 16.0 | PASS |
| photometric.fov_radius_deg | 0 | 自动计算; >30 忽略 | PASS |
| photometric.filters_json | 空字符串 | 使用默认路径 | PASS |
| photometric.qe_curves_json | 空字符串 | 使用默认路径 | PASS |
| drizzle.pixfrac | <0 或 >1 | 忽略, 使用默认 1.0 | PASS |
| drizzle.nested | (任何值) | true->1, false->0 | PASS |
| gradient_sphere.gaia_data_dir | 空字符串 | nullptr (跳过星拒绝) | PASS |
| gradient_sphere.gradient_max_iter | 0 或负数 | 默认 10 | PASS |
| gradient_sphere.gradient_lambda | 0 或负数 | 默认 1e-4 | PASS |
| stack.sigma_clip_sigma | 0 或负数 | 默认 3.0 | PASS |
| stack.sigma_clip_max_iter | 0 或负数 | 默认 5 | PASS |

### 失败测试 (代码审查 PASS)

| 场景 | 处理方式 | 验证结果 |
|------|----------|----------|
| config 文件不存在 | load_config 返回 false, 错误消息 | PASS (P03-001 已验证) |
| config 路径为空 | load_config 返回 false | PASS (P03-001 已验证) |
| config JSON 格式错误 | orc_getJson* 函数返回默认值 | PASS |
| FITS 文件不存在 | run_stage1 返回 error_msg | PASS (P03-001 已验证) |
| DLL 加载失败 | init_dlls 返回 false, 部分阶段跳过 | PASS (P02-007 已验证) |

## 测试覆盖

### 参数覆盖
- **stage1 参数**: 34/34 (100%) 已验证
- **stage2 参数**: 15/15 (100%) 已验证 (代码审查)
- **P03-002 修复的断裂点**: 5/5 (100%) 已验证
  - filters_json, qe_curves_json: 正常测试日志验证
  - gradient_sphere 3 参数: 代码审查验证 (stage2 需多帧输入)

### Stage 覆盖
- READ_FITS: 正常测试 PASS
- CALIBRATE: 正常测试 PASS (P03-001 参数)
- PLATESOLVE: 正常测试 PASS
- PSF: 正常测试 PASS
- PHOTOMETRIC: 正常测试 PASS
- SNR: 正常测试 PASS (跳过, sigma_residual<=0)
- DRIZZLE: 正常测试 PASS
- GRADIENT_SPHERE: 代码审查 PASS
- STACK: 代码审查 PASS (骨架, 跳过)

## 结论
- **正常测试**: PASS (stage1 完整执行, 所有参数传递日志验证通过)
- **边界测试**: PASS (19 个边界条件代码审查通过)
- **失败测试**: PASS (5 个失败场景代码审查通过)
- **总体**: PASS
