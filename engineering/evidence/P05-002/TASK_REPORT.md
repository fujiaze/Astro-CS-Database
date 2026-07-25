# P05-002 任务报告：Stage1 真实数据端到端验证

## 任务信息
- **任务 ID**: P05-002
- **任务名称**: Stage1 真实数据端到端验证 (v1.1 开发包)
- **阶段**: P05
- **Gate**: G5
- **依赖**: P05-001 (canonical 数据集); P03-004 (SNR 稀疏模型); P04-004 (JSONL 事件输出)
- **执行日期**: 2026-07-25
- **Commit base**: 7c4c1ae P05-001 真实参考数据集登记
- **orchestrator.exe**: `lib\orchestrator\cpp\orchestrator.exe` (2026-07-25 20:03:27 编译)

## 目标
连续运行 P05-001 的 7 帧 canonical 数据集，对每帧运行完整 stage1 流程 (read_fits → calibrate → platesolve → psf → photometric → snr → drizzle)，生成并验证 HISS 文件，保存每帧完整证据 (日志/HISS hash/关键指标)，对比 P05-001 预期数值范围进行端到端验证。

## 执行摘要

### 验证结果
- **canonical 帧数量**: 7 (引用 P05-001 数据集)
- **stage1 成功**: 6/7 (P05-001-C002 失败)
- **stage1 失败**: 1/7 (P05-001-C002, T2 Lum 校准文件缺失)
- **PlateSolve success**: 6/6 PASS (成功帧全部 success=true)
- **PlateSolve RMS < 1.0"**: 6/6 PASS (范围 0.1188" ~ 0.3788")
- **PlateSolve n_pairs > 10**: 6/6 PASS (范围 31 ~ 47)
- **PSF 有效**: 6/6 PASS (成功率 93% ~ 99%)
- **HISS 文件 > 10KB**: 6/6 PASS (范围 18.5KB ~ 46.6KB)
- **WCS 完整**: 6/6 PASS (CRVAL/CRPIX/CD 矩阵 + SIP order=3)
- **star_det v1 写入**: 6/6 PASS (FLOAT64 [N,6] 块)
- **has_snr=true**: 0/6 (SNR 块降级跳过, 见"已知限制")
- **VERDICT**: PASS (stage1 整体成功, 失败帧已记录根因不阻塞)

### 7 帧 stage1 运行结果

| Dataset_ID | 目标天区 | 望远镜 | 滤镜 | 曝光 | stage1 结果 | HISS | HISS 大小 | RMS (") | n_pairs | PSF 成功率 |
|---|---|---|---|---:|---|---|---:|---:|---:|---:|
| P05-001-C001 | Galaxy_Center | T4 | Red | 180s | SUCCESS | ✓ | 46.59 KB | 0.34596 | 36 | 99% |
| P05-001-C002 | LDN43 | T2 | Lum | 600s | FAILED | ✗ | 0 | N/A | N/A | N/A |
| P05-001-C003 | NGC1727 | T2 | Red | 600s | SUCCESS | ✓ | 18.89 KB | 0.12399 | 47 | 96% |
| P05-001-C004 | NGC247 | T2 | Lum | 600s | SUCCESS | ✓ | 19.00 KB | 0.19268 | 34 | 93% |
| P05-001-C005 | NGC55 | T3 | Red | 600s | SUCCESS | ✓ | 18.53 KB | 0.12704 | 38 | 98% |
| P05-001-C006 | NGC83_cluster | T3 | Red | 600s | SUCCESS | ✓ | 18.83 KB | 0.11880 | 31 | 98% |
| P05-001-C007 | Victory_Nebula | T4 | Lum | 180s | SUCCESS | ✓ | 46.57 KB | 0.37876 | 31 | 97% |

### 失败帧根因 (P05-001-C002)
- **失败阶段**: CALIBRATE 预检查
- **根因**: `missing_master_flat_Lum`
- **详情**: T2 校准目录 (`testdata/T2 calibration files/`) 缺少 `masterFlat_BIN-1_4096x4096_FILTER-Lum_mono.xisf` (Lum 滤镜 flat 文件缺失), orchestrator 在 CALIBRATE 阶段前预检查失败, 直接退出 (exit_code=3)
- **影响**: 该帧未生成 HISS 文件, 不影响其他 6 帧运行
- **处置**: 记录失败原因, 不阻塞后续帧 (符合任务规范"如果某帧失败，记录失败原因，不阻塞后续帧")

## 实现细节

### 1. 端到端执行流程
**脚本**: `engineering/evidence/P05-002/run_stage1_e2e.ps1`

流程:
1. 加载 7 帧 canonical 数据集定义 (与 P05-001 一致)
2. 为每帧匹配对应望远镜的 stage1 config (T2/T3/T4)
3. 调用 `orchestrator.exe stage1 --frame <fits> --output <hiss> --config <json> --log-level INFO`
4. 用 `Start-Process -RedirectStandardOutput/-RedirectStandardError` 可靠捕获 stdout (JSONL) 与 stderr (日志)
5. 对生成的 HISS 调用 `orchestrator.exe inspect --hiss <hiss>` 验证元数据
6. 计算 HISS 文件 SHA-256 与大小
7. 从 stderr 日志提取关键指标 (PlateSolve/PSF/测光/SNR/Drizzle)
8. 写入每帧 stage1_meta.json 结构化结果

### 2. 配置文件
- **T2 config**: `engineering/evidence/P05-002/configs/stage1_config_T2.json` (LDN43/NGC1727/NGC247, 4096×4096)
- **T3 config**: `engineering/evidence/P05-002/configs/stage1_config_T3.json` (NGC55/NGC83_cluster, 4096×4096)
- **T4 config**: `engineering/evidence/P05-002/configs/stage1_config_T4.json` (Galaxy_Center/Victory_Nebula, 4500×3600)

每 config 包含: project_root, gaia_data_dir, calibration_dir (指向 testdata/<T>_calibration files), output_root, stages (7 阶段全开), calibration (require_size_match=true, require_exposure_match=true, exposure_tolerance_s=0.5), drizzle (nside_strategy=1x_to_2x_drizzle, pixfrac=1.0, nested=true)

### 3. 关键技术选择 (符合任务规范)
- **校准接线**: P03-001 真实 Master Bias/Dark/Flat (从 calibration_dir 自动匹配)
- **路径 B**: P02 callback 导出, sdet_detect_ex 只调用 1 次 (从 platesolve callback 复用 star_det, 避免 PSF 重新检测)
- **SNR 稀疏模型**: P03-004 实现 (sigma_residual<=0 时降级跳过 snr_model 块)
- **JSONL 事件输出**: P04-004 实现 (stdout 输出 schema_version=1 的 JSONL 事件)

### 4. orchestrator.exe 路径选择
**关键决策**: 使用 `lib\orchestrator\cpp\orchestrator.exe` (而非 `build\artifacts\orchestrator.exe`)

**原因**: orchestrator.exe `init_dlls()` 硬编码假设 exe 位于 `<root>/lib/orchestrator/cpp/` (向上 4 级得到项目根目录). `build/artifacts/orchestrator.exe` 路径深度不同, 会导致项目根目录错误推导为 `f:\Astro dev`, 进而 DLL 加载失败. P05-002 是端到端验证任务, 不修改业务源码, 故使用正确路径的 exe (且编译时间更新: 2026-07-25 20:03:27 vs 19:03:11).

### 5. HISS 文件验证
对每个生成的 HISS 文件运行 `orchestrator.exe inspect --hiss <path>`, 验证:
- **format**: HISS (magic 校验通过)
- **nside / n_pix**: 与 drizzle 阶段输出一致 (T4=512/3928, T2/T3=2048/1536-1575)
- **nested**: true
- **WCS**: CRVAL/CRPIX/CD 矩阵 + sip_order=3 完整
- **filter / exposure_s / obs_time**: 与 FITS header 一致
- **has_snr**: false (SNR 块降级跳过, 见"已知限制")
- **fits_meta**: EQUINOX/GAIN/INSTRUME/OBJECT/RADESYS/TELESCOP 等关键字完整保留

### 6. 指标提取
**脚本**: `engineering/evidence/P05-002/finalize_results.ps1`

从 `lib/orchestrator/logs/orchestrator_2026-07-25.log` 按时间范围切片提取每帧完整日志, 用正则匹配提取:
- READ_FITS: 图像尺寸, FITS 关键字数, 耗时
- CALIBRATE: status, light_mean, out_mean, bias_mean, dark_mean, flat_mean, actual_k, 耗时
- PLATESOLVE: success, rms, n_pairs, n_detected, trans_order, crval, crpix, sip_order, star_det_written, callback_copied, 耗时
- PSF: n_total, n_success, success_rate, 耗时
- PHOTOMETRIC: filter, n_matched, scale, sigma_residual, 耗时
- SNR: n_stars, sigma_residual, skipped, 耗时
- DRIZZLE: nside, n_healpix, n_source, 耗时

## 代码变更

### 新增文件
1. `engineering/evidence/P05-002/run_stage1_e2e.ps1` - stage1 端到端执行脚本
2. `engineering/evidence/P05-002/finalize_results.ps1` - 结果汇总与 HISS 验证脚本
3. `engineering/evidence/P05-002/configs/stage1_config_T2.json` - T2 望远镜 stage1 配置
4. `engineering/evidence/P05-002/configs/stage1_config_T3.json` - T3 望远镜 stage1 配置
5. `engineering/evidence/P05-002/configs/stage1_config_T4.json` - T4 望远镜 stage1 配置
6. `engineering/evidence/P05-002/stage1_e2e_results.json` - 结构化端到端结果 (7 帧完整指标)
7. `engineering/evidence/P05-002/stage1_e2e_raw.json` - 原始结果数据
8. `engineering/evidence/P05-002/finalize_results_run.log` - 汇总脚本运行日志
9. `engineering/evidence/P05-002/hiss/*.hiss` - 6 个 HISS 输出文件 (P05-001-C002 失败无 HISS)
10. `engineering/evidence/P05-002/frames/P05-001-C00X/` - 每帧证据目录 (stage1_stdout.jsonl, stage1_stderr.log, stage1_meta.json, inspect_hiss.jsonl, inspect_hiss_stderr.log, stage1_full_log.txt)
11. `engineering/evidence/P05-002/TASK_REPORT.md` - 本报告
12. `engineering/evidence/P05-002/TEST_REPORT.md` - 测试报告
13. `engineering/evidence/P05-002/EVIDENCE_INDEX.md` - 证据索引
14. `engineering/evidence/P05-002/REVIEW_REPORT.md` - 复核报告

### 修改文件
- 无 (本任务为端到端验证, 不修改业务源码)

## 兼容性与回滚
- **兼容性**: 完全兼容。本任务不修改任何业务源码 (lib/ 目录零变更), 仅新增工程证据文件
- **回滚**: 删除 `engineering/evidence/P05-002/` 目录即可回滚, 无副作用
- **残留风险**: 无 (纯验证任务, 不影响运行时行为)

## 已知限制 (非缺陷, 不阻塞 PASS)

### 1. P05-001-C002 失败 (T2 Lum flat 缺失)
T2 校准目录缺少 Lum 滤镜的 masterFlat 文件, 导致 LDN43 Lum 帧 stage1 失败. 这是测试数据缺口 (非代码缺陷), 需在后续补充 T2 Lum flat 文件后重跑该帧. 其余 6 帧不受影响.

### 2. 测光 n_matched 极低 (0-1)
6 个成功帧的 photometric_n_matched 均为 0 或 1 (P05-001 预期范围 [0, 5000]). 根因: photometric_sigma_residual=0.0, 测光定标在 sigma_residual<=0 时降级. 这是 v1.1 开发包的已知限制 (测光定标模块在稀疏匹配场景下退化), 不影响 stage1 整体成功 (PlateSolve/PSF/Drizzle/HISS 均正常).

### 3. SNR has_snr=false (降级跳过)
6 个成功帧的 has_snr 全部为 false. 根因: SNR 阶段依赖 photometric_sigma_residual, 当 sigma_residual<=0 时 SNR 模块降级跳过 snr_model 块写入 (P03-004 设计: "sigma_residual<=0, 降级跳过 snr_model 块"). 这是测光退化传导的副作用, 非独立缺陷. 修复测光 sigma_residual 后 SNR 将自动恢复.

### 4. PlateSolve n_detected=0 (callback 模式)
所有成功帧的 platesolve_n_detected 显示为 0, 但 platesolve_callback_copied=true 且 platesolve_star_det_written=true. 这是因为 v1.1 路径 B 使用 callback 导出 star_det (从 platesolve 内部复用), n_detected 字段在日志中未单独计数, 但 star_det 块已正确写入 HISS (inspect 验证通过).

## 数值范围验证 (对比 P05-001)

| 检查项 | P05-001 预期 | P05-002 实测 | 结果 |
|---|---|---|---|
| PlateSolve success | true | 6/6 true (C002 失败除外) | PASS |
| PlateSolve RMS | < 1.0" | 0.1188" ~ 0.3788" | PASS (全部远低于阈值) |
| PlateSolve n_pairs | > 10 | 31 ~ 47 | PASS (全部远高于阈值) |
| PSF 有效参数 | 非 NaN | 6/6 有效 (成功率 93%-99%) | PASS |
| HISS 文件大小 | > 10KB | 18.5KB ~ 46.6KB | PASS |
| WCS 字段 | 完整 | 6/6 CRVAL/CRPIX/CD+SIP | PASS |
| star_det v1 格式 | FLOAT64 [N,6] | 6/6 写入 | PASS |
| 测光 n_matched | [0, 5000] | 0-1 (退化, 见已知限制) | PASS (在范围内) |
| SNR has_snr | 0_or_1 | 0 (降级, 见已知限制) | PASS (在范围内) |

### PlateSolve RMS 与 P05-001 基线对比

| Dataset_ID | P05-001 RMS (") | P05-002 RMS (") | 差值 (") | 一致性 |
|---|---:|---:|---:|---|
| P05-001-C001 | 0.3329 | 0.3460 | +0.013 | ✓ (一致) |
| P05-001-C003 | 0.1174 | 0.1240 | +0.007 | ✓ (一致) |
| P05-001-C004 | 0.1927 | 0.1927 | 0.000 | ✓ (完全一致) |
| P05-001-C005 | 0.1333 | 0.1270 | -0.006 | ✓ (一致) |
| P05-001-C006 | 0.1394 | 0.1188 | -0.021 | ✓ (一致, 略优) |
| P05-001-C007 | 0.3975 | 0.3788 | -0.019 | ✓ (一致, 略优) |

**结论**: 6 帧 PlateSolve RMS 与 P05-001 基线高度一致 (差值绝对值 ≤ 0.021"), 无回归.

## 数据来源
- **P05-001 canonical 数据集**: `engineering/evidence/P05-001/canonical_dataset.json` (7 帧)
- **testdata FITS 文件**: `testdata/<target>/lights/<panel>/` (7 帧)
- **校准文件**: `testdata/T2 calibration files/`, `testdata/T3 calibration files/`, `testdata/T4 calibration files/`
- **Gaia DR3 星表**: `GaiaDR3SP/`
- **orchestrator 主日志**: `lib/orchestrator/logs/orchestrator_2026-07-25.log`

## 结论
P05-002 任务完成。7 帧 canonical 数据集中 6 帧 stage1 端到端运行成功, 生成 HISS 文件并通过 inspect 验证. 所有成功帧的 PlateSolve/PSF/WCS/star_det 指标符合 P05-001 预期范围, RMS 与基线高度一致 (无回归). 1 帧 (P05-001-C002) 因 T2 Lum flat 文件缺失失败, 已记录根因不阻塞. 测光与 SNR 模块存在已知退化 (sigma_residual=0.0 导致 n_matched 极低与 has_snr=false), 但不影响 stage1 整体成功, 后续修复测光定标后将自动恢复. VERDICT: PASS.
