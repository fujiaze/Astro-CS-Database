# 代码审计问题清单 - P3（Low，可选修复）

> 配套总报告：`2026-07-18-code-audit-report.md`
> 本文档包含 9 模块所有 Low（59 项）问题
> Low 级别问题主要是代码风格、注释、命名、magic number 等不影响功能的小问题
> 用户可快速浏览决定是否批量批复"暂不修复"或挑选部分修复
> 术语表见 P0+P1 文档顶部

---

## 目录

- [B1 astro_image_io (8 项)](#b1-astro_image_io)
- [B2 calibration (5 项)](#b2-calibration)
- [B3 plate_solve (3 项)](#b3-plate_solve)
- [B4 dynamic_psf (5 项)](#b4-dynamic_psf)
- [B5 photometric_calib (6 项)](#b5-photometric_calib)
- [B6 snr_estimator (6 项)](#b6-snr_estimator)
- [B7 healpix_drizzle (8 项)](#b7-healpix_drizzle)
- [B8 healpix_stack (7 项)](#b8-healpix_stack)
- [B9 orchestrator (11 项)](#b9-orchestrator)

---

## B1 astro_image_io

### B1-L-1: 代码风格不一致（部分文件用 4 空格，部分用 tab）

**问题定位**：`lib/astro_image_io/src/*.cpp` 多文件
**问题描述**：模块内部分文件用 4 空格缩进，部分用 tab，混用影响可读性。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B1-L-2: 函数注释不全（参数含义、返回值未说明）

**问题定位**：`lib/astro_image_io/src/aio_api.cpp` 多处对外 API 函数
**问题描述**：对外 C API 函数的参数含义、返回值、错误码未在注释中说明。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B1-L-3: 变量命名混用（局部变量 camelCase / snake_case 不统一）

**问题定位**：`lib/astro_image_io/src/aio_pipeline.cpp` 等
**问题描述**：局部变量命名风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B1-L-4: magic number 未提取为常量

**问题定位**：`lib/astro_image_io/src/aio_fits.cpp` 等
**问题描述**：如 2880（FITS 块大小）、80（关键字长度）等 magic number 直接写在代码中，未提取为命名常量。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B1-L-5: 头文件 include 顺序不规范

**问题定位**：`lib/astro_image_io/src/*.cpp`
**问题描述**：头文件 include 顺序不规范（应系统头、第三方头、项目头分组）。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B1-L-6: 错误信息字符串不一致（中英文混用）

**问题定位**：`lib/astro_image_io/src/*.cpp` 多处
**问题描述**：错误信息有的用英文有的用中文，影响日志归档分析。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B1-L-7: TODO 注释残留

**问题定位**：`lib/astro_image_io/src/*.cpp` 多处
**问题描述**：代码中残留多处 TODO/FIXME 注释未清理。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B1-L-8: 文件头注释缺失

**问题定位**：`lib/astro_image_io/src/aio_compressor.cpp` 等
**问题描述**：部分文件无文件头注释（功能描述、作者、日期）。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B2 calibration

### B2-L-1: 代码风格不一致

**问题定位**：`lib/calibration/src/*.cpp`
**问题描述**：模块内代码风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B2-L-2: 函数注释不全

**问题定位**：`lib/calibration/src/ac_api.cpp`
**问题描述**：对外 API 函数注释不全。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B2-L-3: 变量命名混用

**问题定位**：`lib/calibration/src/calibrator.cpp`
**问题描述**：变量命名风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B2-L-4: magic number 未提取

**问题定位**：`lib/calibration/src/cosmetic_corrector.cpp`
**问题描述**：如坏点识别阈值 3.0、5.0 等 magic number 直接写代码。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B2-L-5: 头文件 include 顺序不规范

**问题定位**：`lib/calibration/src/*.cpp`
**问题描述**：include 顺序不规范。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B3 plate_solve

### B3-L-1: 代码风格不一致

**问题定位**：`lib/plate_solve/cpp/ipv/src/*.cpp`
**问题描述**：模块内代码风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B3-L-2: 算法原理注释缺失（如 RANSAC 数学推导）

**问题定位**：`lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp` 等
**问题描述**：RANSAC、Umeyama SVD 等算法的数学推导、参数选择依据在代码中无注释。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B3-L-3: 变量命名混用

**问题定位**：`lib/plate_solve/cpp/ipv/src/*.cpp`
**问题描述**：变量命名风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B4 dynamic_psf

### B4-L-1: 代码风格不一致

**问题定位**：`lib/dynamic_psf/src/*.cpp`
**问题描述**：模块内代码风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B4-L-2: 函数注释不全

**问题定位**：`lib/dynamic_psf/src/dpsf_psf.cpp`
**问题描述**：Moffat4 函数原型、拟合算法注释不全。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B4-L-3: 变量命名混用

**问题定位**：`lib/dynamic_psf/src/*.cpp`
**问题描述**：变量命名风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B4-L-4: magic number 未提取

**问题定位**：`lib/dynamic_psf/src/dpsf_psf.cpp`
**问题描述**：如 patch 大小 17、最大迭代 100 等 magic number。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B4-L-5: 文件头注释缺失

**问题定位**：`lib/dynamic_psf/src/dpsf_log.cpp`
**问题描述**：部分文件无文件头注释。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B5 photometric_calib

### B5-L-1: 代码风格不一致

**问题定位**：`lib/photometric_calib/cpp/src/*.cpp`
**问题描述**：模块内代码风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B5-L-2: 函数注释不全

**问题定位**：`lib/photometric_calib/cpp/src/pc_api.cpp`
**问题描述**：对外 API 函数注释不全。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B5-L-3: 变量命名混用

**问题定位**：`lib/photometric_calib/cpp/src/*.cpp`
**问题描述**：变量命名风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B5-L-4: magic number 未提取

**问题定位**：`lib/photometric_calib/cpp/src/spectrum_integrator.cpp`
**问题描述**：如积分步长 1.0nm、波长范围 380-780nm 等 magic number。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B5-L-5: 注释不全

**问题定位**：`lib/photometric_calib/cpp/src/image_corrector.cpp`
**问题描述**：图像校正逻辑注释不全。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B5-L-6: 文件头注释缺失

**问题定位**：`lib/photometric_calib/cpp/src/wcs_transform.cpp`
**问题描述**：部分文件无文件头注释。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B6 snr_estimator

### B6-L-1: 代码风格不一致

**问题定位**：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
**问题描述**：模块内代码风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B6-L-2: 函数注释不全

**问题定位**：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
**问题描述**：SNR 估算公式、IDW 插值算法注释不全。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B6-L-3: 变量命名混用

**问题定位**：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
**问题描述**：变量命名风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B6-L-4: magic number 未提取

**问题定位**：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
**问题描述**：如控制点采样密度、稀疏化阈值等 magic number。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B6-L-5: 注释不全

**问题定位**：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
**问题描述**：乘法模型 SNR = SNR_phot × (IDW/median) 的数学推导无注释。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B6-L-6: 文件头注释缺失

**问题定位**：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
**问题描述**：文件头注释简陋。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B7 healpix_drizzle

### B7-L-1: 代码风格不一致

**问题定位**：`lib/healpix_db/healpix_drizzle/*.cpp`
**问题描述**：模块内代码风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B7-L-2: 函数注释不全

**问题定位**：`lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp`
**问题描述**：对外 API 函数注释不全。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B7-L-3: 变量命名混用

**问题定位**：`lib/healpix_db/healpix_drizzle/*.cpp`
**问题描述**：变量命名风格不统一。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B7-L-4: magic number 未提取

**问题定位**：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
**问题描述**：如 drizzle_factor 1.5、nside 默认 32768 等 magic number。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B7-L-5: 注释不全

**问题定位**：`lib/healpix_db/healpix_drizzle/poly_clip.cpp`
**问题描述**：多边形裁剪算法（Sutherland-Hodgman）的算法原理无注释。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B7-L-6: 文件头注释缺失

**问题定位**：`lib/healpix_db/healpix_drizzle/wcs_sip.cpp`
**问题描述**：部分文件无文件头注释。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B7-L-7: TODO 注释残留

**问题定位**：`lib/healpix_db/healpix_drizzle/*.cpp`
**问题描述**：代码中残留 TODO 注释。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B7-L-8: 错误信息字符串不一致

**问题定位**：`lib/healpix_db/healpix_drizzle/*.cpp`
**问题描述**：错误信息中英文混用。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B8 healpix_stack

### B8-L-1: 数据块读取失败返回空 vector 语义不清晰

**问题定位**：`lib/healpix_db/healpix_stack/ahps_reader.cpp:313-350`
**问题描述**：readChunk 解压失败时返回空 vector，调用方需检查 empty() 区分"解压失败"与"块大小为 0"，语义不清晰。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B8-L-2: 子叶块列表仅实现 Windows 路径

**问题定位**：`lib/healpix_db/healpix_stack/stack_db.cpp:376-379`
**问题描述**：listTiles 函数的 POSIX 分支注释"此处省略，主要平台为 Windows"，Linux/macOS 上返回空。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B8-L-3: 元数据保存用 %g 格式可能丢精度

**问题定位**：`lib/healpix_db/healpix_stack/stack_db.cpp:229-254`
**问题描述**：saveMeta 用 snprintf + %g 写 sigmaClipLow/High，可能将 3.0 写为 "3"，JSON 格式不规范。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B8-L-4: Winsorized 分支内存占用高

**问题定位**：`lib/healpix_db/healpix_stack/gradient/corrected_stacker.cpp:217-244`
**问题描述**：Winsorized 分支每轮迭代创建二维 vector 收集每像素的帧值，50 帧 × 100 万像素内存峰值 400MB。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B8-L-5: gauge fixing 假设 v[0] 是常数项，依赖实现细节

**问题定位**：`lib/healpix_db/healpix_stack/gradient/gradient_fitter.cpp:252-276`
**问题描述**：gauge fixing 从 v[0] 减去 offset，假设 v[0] 是球面样条的常数项，依赖 spherical_spline 零空间构造。若实现更改会失效。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B8-L-6: run_stage_stack 返回 true 但未执行任何工作

**问题定位**：`lib/orchestrator/cpp/src/orchestrator.cpp:2462-2476`
**问题描述**：（与 B8-H-3 / B9-H-4 同源）run_stage_stack 返回 true 但实际未工作，stage2 成功标志永远为 true，无法反映 STACK 实际状态。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B8-L-7: 大量 fprintf(stderr) 未走统一日志系统

**问题定位**：`lib/healpix_db/healpix_stack/` 全模块多处
**问题描述**：（与 B1-H-1 同类）模块大量用 fprintf(stderr)，无模块日志目录，无法按级别过滤、归档分析。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## B9 orchestrator

### B9-L-1: 旧版 stage_name 函数缺少新节点分支

**问题定位**：`lib/orchestrator/cpp/src/orchestrator.cpp:200-209`
**问题描述**：stage_name 函数只覆盖 5 个旧枚举值，调用 stage_name(READ_FITS) 等返回 "UNKNOWN"。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-2: --threads 参数未生效

**问题定位**：`lib/orchestrator/cpp/src/cli_command.cpp:286-289`
**问题描述**：cmd_run 的 --threads 参数仅记录日志，未传递到 orchestrator.config_.threads。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-3: 输出 JSON 字段名 output_ahpx_path 过时

**问题定位**：
- `lib/orchestrator/cpp/src/cli_command.cpp:495`
- `lib/orchestrator/cpp/include/orchestrator.h:92`

**问题描述**：JSON 输出字段名 "output_ahpx_path"，但实际输出是 .hiss 或 .hcsd（.ahpx 已废弃），字段名误导。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-4: cmd_status 输出固定"no running instance"

**问题定位**：`lib/orchestrator/cpp/src/cli_command.cpp:369-375`
**问题描述**：单次命令模式下 status 只输出固定 JSON，无实际状态查询。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-5: cmd_run_batch 错误返回码 4 不规范

**问题定位**：`lib/orchestrator/cpp/src/cli_command.cpp:336`
**问题描述**：`return 4;  // 目录不存在错误`，但其他错误返回 1/2/3，无统一错误码体系。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-6: PLATESOLVE 中 FOV 半径上限 30 度硬编码

**问题定位**：`lib/orchestrator/cpp/src/orchestrator.cpp:1508`
**问题描述**：`if (fov_radius_deg > 0.0 && fov_radius_deg < 30.0)` 30 度硬编码无注释依据。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-7: parse_ra_hms / parse_dec_dms 解析失败返回 0.0 无法区分

**问题定位**：`lib/orchestrator/cpp/src/orchestrator.cpp:793-831`
**问题描述**：解析失败返回 0.0，调用方无法区分"OBJCTRA 缺失/格式错误"和"实际 RA=0"，可能导致用错误的 (0,0) 初始指向。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-8: --threads 参数日志说"骨架"但实际已部分支持

**问题定位**：`lib/orchestrator/cpp/src/cli_command.cpp:288`
**问题描述**：日志说"骨架: 配置传递待完善"，但 init_dlls 实际已对 CALIBRATE 模块生效，日志过时。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-9: cmd_stage1 中 gaia_data/calibration_dir/filter_name 参数仅记录日志未使用

**问题定位**：`lib/orchestrator/cpp/src/cli_command.cpp:212-215`
**问题描述**：CLI 参数仅记录日志，未传递到 OrchestratorConfig，参数无效。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-10: cmd_run_batch 返回码逻辑：results 为空时返回 0

**问题定位**：`lib/orchestrator/cpp/src/cli_command.cpp:359-363`
**问题描述**：`for (const auto& r : results) { if (!r.success) return 3; }` 当 results 为空（目录无 FITS）时返回 0，误导用户认为成功。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

### B9-L-11: tests/ 目录缺少 stage handler 单元测试

**问题定位**：`lib/orchestrator/cpp/tests/`
**问题描述**：缺少 stage handler（run_stage_calibrate/platesolve/psf/photometric/snr/drizzle）的单元测试，stage handler 修改无回归保障。
**批复**：- [ ] 同意修复 - [ ] 刻意为之 - [ ] 暂不修复 - 备注：

---

## 批复汇总表

> Low 级问题主要是代码风格、注释、命名等，用户可批量批复"暂不修复"或挑选部分修复。

| 问题 ID | 模块 | 批复 | 备注 |
|---------|------|------|------|
| B1-L-1 | astro_image_io | | |
| B1-L-2 | astro_image_io | | |
| B1-L-3 | astro_image_io | | |
| B1-L-4 | astro_image_io | | |
| B1-L-5 | astro_image_io | | |
| B1-L-6 | astro_image_io | | |
| B1-L-7 | astro_image_io | | |
| B1-L-8 | astro_image_io | | |
| B2-L-1 | calibration | | |
| B2-L-2 | calibration | | |
| B2-L-3 | calibration | | |
| B2-L-4 | calibration | | |
| B2-L-5 | calibration | | |
| B3-L-1 | plate_solve | | |
| B3-L-2 | plate_solve | | |
| B3-L-3 | plate_solve | | |
| B4-L-1 | dynamic_psf | | |
| B4-L-2 | dynamic_psf | | |
| B4-L-3 | dynamic_psf | | |
| B4-L-4 | dynamic_psf | | |
| B4-L-5 | dynamic_psf | | |
| B5-L-1 | photometric_calib | | |
| B5-L-2 | photometric_calib | | |
| B5-L-3 | photometric_calib | | |
| B5-L-4 | photometric_calib | | |
| B5-L-5 | photometric_calib | | |
| B5-L-6 | photometric_calib | | |
| B6-L-1 | snr_estimator | | |
| B6-L-2 | snr_estimator | | |
| B6-L-3 | snr_estimator | | |
| B6-L-4 | snr_estimator | | |
| B6-L-5 | snr_estimator | | |
| B6-L-6 | snr_estimator | | |
| B7-L-1 | healpix_drizzle | | |
| B7-L-2 | healpix_drizzle | | |
| B7-L-3 | healpix_drizzle | | |
| B7-L-4 | healpix_drizzle | | |
| B7-L-5 | healpix_drizzle | | |
| B7-L-6 | healpix_drizzle | | |
| B7-L-7 | healpix_drizzle | | |
| B7-L-8 | healpix_drizzle | | |
| B8-L-1 | healpix_stack | | |
| B8-L-2 | healpix_stack | | |
| B8-L-3 | healpix_stack | | |
| B8-L-4 | healpix_stack | | |
| B8-L-5 | healpix_stack | | |
| B8-L-6 | healpix_stack | | |
| B8-L-7 | healpix_stack | | |
| B9-L-1 | orchestrator | | |
| B9-L-2 | orchestrator | | |
| B9-L-3 | orchestrator | | |
| B9-L-4 | orchestrator | | |
| B9-L-5 | orchestrator | | |
| B9-L-6 | orchestrator | | |
| B9-L-7 | orchestrator | | |
| B9-L-8 | orchestrator | | |
| B9-L-9 | orchestrator | | |
| B9-L-10 | orchestrator | | |
| B9-L-11 | orchestrator | | |

---

**统计**：Low 59 项

---

## 批量批复建议

如果用户希望快速处理 Low 级问题，可考虑以下批量方案：

- **方案 A**：全部"暂不修复"（保持现状，专注于 P0+P1+P2 修复）
- **方案 B**：仅修复"注释类"问题（B1-L-2/L-7/L-8、B3-L-2、B4-L-2/L-5、B5-L-2/L-5/L-6、B6-L-2/L-5/L-6、B7-L-2/L-5/L-6/L-7），其余暂不修复
- **方案 C**：仅修复"magic number"问题（B1-L-4、B2-L-4、B4-L-4、B5-L-4、B6-L-4、B7-L-4），其余暂不修复
- **方案 D**：逐项审阅批复

请用户在汇总表中填写批复决定，或直接告知批量方案。
