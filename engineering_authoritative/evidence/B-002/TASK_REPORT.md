# B-002 任务执行报告 — 代表帧 Stage1 与科学统计

- 任务编号: B-002
- 执行日期: 2026-07-30
- 执行环境: PowerShell 7, MSYS2 mingw64 g++ (PATH 含 C:\msys64\mingw64\bin)
- orchestrator: lib\orchestrator\cpp\orchestrator.exe (2026-07-29 构建)
- 配置来源: engineering_authoritative\evidence\B-001\configs\stage1_config_{T2,T3,T4}_Red.json
- 输出目录: output\B-002\ (HISS) / engineering_authoritative\evidence\B-002\ (报告与日志)

## 1. DLL 编译状态

**结论: 无需编译 — 任务前提与实际不符。**

任务描述称 3 个 DLL 缺失需要编译，但实际检查发现 orchestrator 依赖的全部业务 DLL 均已存在且为近期构建（2026-07-25 ~ 2026-07-28）。任务描述中的 DLL 名称为简称，与 orchestrator 实际加载名（见 lib\orchestrator\cpp\src\dll_loader.cpp）对应关系如下：

| 任务简称 | orchestrator 实际加载名 | 路径 | 存在 | 构建时间 |
|---|---|---|---|---|
| calibration.dll | astro_calibration.dll | lib\calibration\ | YES | 2026-07-25 15:10 |
| ipv.dll | ipv_solver.dll | lib\plate_solve\cpp\ipv\ | YES | 2026-07-28 19:12 |
| snr_evaluator.dll | snr_estimator.dll | lib\snr_estimator\cpp\ | YES | 2026-07-25 16:37 |

其余依赖 DLL（astro_image_io / dynamic_psf / photometric_calib / gaia_client / healpix_drizzle / healpix_stack）同样齐全。因此跳过编译步骤，直接进入 Stage1 运行。各 DLL 构建脚本（Makefile + build.ps1）已在各自目录就位，如后续需要重建可直接调用。

## 2. 三帧 Stage1 运行结果

运行命令模板：
```
orchestrator.exe stage1 --frame <fits> --output output/B-002/<id>.hiss --config <config.json>
```
单帧 wall-clock 均远低于 300s 超时阈值。

| 帧ID | 目标 | 曝光(s) | 输入FITS | 退出码 | 耗时(s) | HISS 输出 | 大小(B) |
|---|---|---|---|---|---|---|---|
| T2_RED_LDN43 | LDN43 | 1200 | (见下注) | 0 | 26.2 | output/B-002/T2_RED_LDN43.hiss | 58076 |
| T3_RED_NGC55 | NGC55 | 600 | testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250703@080546-600S-Red.fts | 0 | 24.4 | output/B-002/T3_RED_NGC55.hiss | 31352 |
| T4_RED_GalaxyCenter_panel1 | Galaxy_Center panel1 | 180 | testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts | 0 | 25.6 | output/B-002/T4_RED_GalaxyCenter_panel1.hiss | 87433 |

**三帧全部成功 (3/3)。**

### 2.1 T2 中文路径编码崩溃与规避（重要）

T2 原始 light_path 含中文字符 "testdata/LDN43_T2**素材**_flying_dutchman/..."。首次运行在 `std::filesystem` 路径构造阶段崩溃：
```
terminate called after throwing an instance of 'std::filesystem::__cxx11::filesystem_error'
  what():  filesystem error: Cannot convert character sequence: Illegal byte sequence
```
根因：mingw g++ 编译的 orchestrator 在 Windows 下以窄字符串构造 `std::filesystem::path` 时，无法在当前 locale 下转换 UTF-8/GBK 多字节中文序列。8.3 短名在该卷被禁用（ShortPath 返回长名），junction 方案因 PowerShell 要求绝对路径 target 也受限。

源码确认（lib\orchestrator\cpp\src\orchestrator.cpp::run_stage1）：`--frame` 参数是实际 FITS 读取路径（`current_fits_path_`），config 中的 `frame.light_path` 仅作日志打印、不参与文件操作。

**规避方案**: 将 T2 FITS 复制到全 ASCII 路径 `output/B-002/input/T2_LDN43_Red_1200s.fts`（Copy-Item 使用 Win32 wide API，中文源路径无问题），以该 ASCII 路径作为 `--frame` 运行。复制后 T2 一次性成功。T3/T4 路径全 ASCII，直接运行无问题。

> 注：output/B-002/input/T2_LDN43_Red_1200s.fts 为规避中文路径的中间副本（33,569,280 字节），保留作为复现证据。

## 3. 各 stage 耗时明细（秒）

| 帧 | READ_FITS | CALIBRATE | PLATESOLVE | PSF | PHOTOMETRIC | SNR | DRIZZLE | 总计 |
|---|---|---|---|---|---|---|---|---|
| T2 | 0.057 | 0.771 | 0.936 | 0.267 | 0.042 | 0.001 | 24.061 | 26.2 |
| T3 | 0.102 | 0.699 | 0.805 | 0.161 | 0.038 | 0.002 | 22.555 | 24.4 |
| T4 | 0.106 | 0.558 | 2.523 | 0.326 | 0.272 | 0.001 | 21.338 | 25.6 |

DRIZZLE 占总耗时 ~90%（nside 自适应 + HEALPix 投影 + HISS 序列化），其余 stage 均在 3s 内完成。

## 4. 警告与备注

- **T4 PlateSolve cd_Δ=6.90%** 偏高（宽场 Nikkor 200mm, FOV≈9.9°），robust_refine 在 iter3 命中 "matched=20 < 30, 回退"，最终停在 iter2（matched=36, rms=0.346"）。宽场大畸变下属预期，RMS 仍优于 0.5" 像素级阈值。
- **DPSF 警告**: 三帧均出现少量 "FWHM exceeds rect" / "Background constraint violated" 单星拟合警告（被剔除），不影响批量拟合成功率（97%~99%）。
- **photometry_report.json 覆盖**: 每个 stage1 运行都将 photometry_report.json 写到 output/B-002\photometry_report.json（同路径覆盖），最终保留的是 T4 的版本。各帧完整 photo_stats 已从 stage1 stdout 的 result JSON 中提取并存入 science_stats.csv。
- 未启动全量 710 帧；未用"文件写出"代替任何测光/SNR/球面计算，所有指标均由 orchestrator 流水线实际执行产生。

## 5. 交付物

- output/B-002/T2_RED_LDN43.hiss
- output/B-002/T3_RED_NGC55.hiss
- output/B-002/T4_RED_GalaxyCenter_panel1.hiss
- engineering_authoritative/evidence/B-002/science_stats.csv
- engineering_authoritative/evidence/B-002/TEST_REPORT.md
- engineering_authoritative/evidence/B-002/TASK_REPORT.md (本文件)
- engineering_authoritative/evidence/B-002/stage1_{T2,T3,T4}_Red.log (三帧完整运行日志)
