# TEST_REPORT

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| Python 环境 | `C:\Users\fujia\AppData\Local\Programs\Python\Python312\python.exe -c "import numpy, astropy; print(numpy.__version__, astropy.__version__)"` | 10s | 0 | PASS | Python 3.12, numpy 1.26.4, astropy 7.1.0 |
| mingw 工具链 | `g++ --version; mingw32-make --version` | 5s | 0 | PASS | g++ 16.1.0 (Rev4, MSYS2), GNU Make 4.4.1 |
| 旧 DLL 基线 SHA-256 | `Get-FileHash build\artifacts\dynamic_psf.dll` | 5s | 0 | PASS | 旧 DLL SHA-256 51AFFB4AD05737B3146274F8FDDB95208327DB336F4A645FF68C4D0032B43BA2 (334677 字节, 构建前) |
| 源码修改后构建 | `mingw32-make clean; mingw32-make` (lib/dynamic_psf/) | 60s | 0 | PASS | DLL 构建成功, 仅 2 个旧代码无关警告 (maxIter/tolerance unused in dpsf_fit) |
| 新 DLL 大小 | `Get-Item dynamic_psf.dll \| Select Length` | 5s | 0 | PASS | 334677 字节 |
| 新 DLL SHA-256 | `Get-FileHash dynamic_psf.dll` | 5s | 0 | PASS | A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A |
| DLL 复制到 build/artifacts | `Copy-Item dynamic_psf.dll ..\..\build\artifacts\` | 5s | 0 | PASS | build/artifacts/dynamic_psf.dll SHA-256 与源一致 (A33286D6...) |
| 旧符号 dpsf_fit 导出 | `python -c "from ctypes import cdll; dll=cdll.LoadLibrary(...); print(hasattr(dll,'dpsf_fit'))"` | 10s | 0 | PASS | True |
| 旧符号 dpsf_fit_batch 导出 | 同上 | 10s | 0 | PASS | True |
| 旧符号 dpsf_free_results 导出 | 同上 | 10s | 0 | PASS | True |
| **新符号 dpsf_fit_batch_f32 导出** | 同上 | 10s | 0 | **PASS** | True (addr=0x000001624581d610) |
| 单帧 A/B 测试 (原始 uint16 数据) | `python engineering\tools\psf_f32_ab_test.py` | 60s | 0 | **PASS** | f32 47/50 有效, u16 47/50 有效, 47 颗两者都成功, 9 字段 \|diff\|=0.000000, 10/10 有效性检查 PASS |
| FITS 读取 | 测试脚本内 astropy.io.fits | 5s | 0 | PASS | 4500×3600 uint16 帧, 像素 [1267, 65535], 中位 1457 |
| star_det v1 生成 | 测试脚本内 numpy 局部最大值检测 | 5s | 0 | PASS | 50 颗星, schema=star_det_v1:FLOAT64[N,6], 0.472s |
| f32 API 调用成功 | DynamicPSF.fit_batch_f32() | 30s | 0 | PASS | 返回 (psf_params[50,9], n_valid=47), 0.016s |
| u16 API 调用成功 (向后兼容) | DynamicPSF.fit_batch() | 30s | 0 | PASS | 返回 50 个 DPSFFitResultPy, 47 个 status=OK, 0.030s |
| f32 输出非 NaN | np.isnan(psf_f32[:,0]).sum() | 5s | 0 | PASS | 47/50 非 NaN (3 颗失败为背景约束违反, 两个 API 都失败) |
| f32 A > 0 | np.all(psf_f32[valid,1] > 0) | 5s | 0 | PASS | 所有有效拟合振幅为正 |
| f32 sx > 0.3 | np.all(psf_f32[valid,4] > 0.3) | 5s | 0 | PASS | sigma_x 下限满足 |
| f32 sy > 0.3 | np.all(psf_f32[valid,5] > 0.3) | 5s | 0 | PASS | sigma_y 下限满足 |
| f32 fwhm_x > 0 | np.all(psf_f32[valid,7] > 0) | 5s | 0 | PASS | FWHM_x 为正 |
| f32 fwhm_y > 0 | np.all(psf_f32[valid,8] > 0) | 5s | 0 | PASS | FWHM_y 为正 |
| f32 fwhm_x < 50 | np.all(psf_f32[valid,7] < 50) | 5s | 0 | PASS | FWHM_x 上限合理 (mean=1.81) |
| f32 fwhm_y < 50 | np.all(psf_f32[valid,8] < 50) | 5s | 0 | PASS | FWHM_y 上限合理 (mean=1.93) |
| f32 B finite | np.all(np.isfinite(psf_f32[valid,0])) | 5s | 0 | PASS | 背景值全部有限 |
| f32 cx in image | np.all((psf_f32[valid,2]>=0) & (psf_f32[valid,2]<4500)) | 5s | 0 | PASS | 中心 x 在图像范围内 |
| f32 cy in image | np.all((psf_f32[valid,3]>=0) & (psf_f32[valid,3]<3600)) | 5s | 0 | PASS | 中心 y 在图像范围内 |
| **HDR 场景 A/B (scale=3.0)** | 内联 Python 脚本 | 60s | 0 | **PASS** | f32 48/50, u16 48/50, A rel_diff_max=3.26 (旧 API 严重低估振幅), fwhm rel_diff_max=0.51 |
| HDR 超 65535 像素统计 | (img_hdr > 65535).sum() | 5s | 0 | PASS | 7295 像素 (0.05%) 超 65535, 旧 API 全部 clip |
| HDR f32 振幅未压缩 | f32 A mean=258985 vs u16 A mean=82492 | 5s | 0 | PASS | 新 API 保留完整动态范围, 旧 API 振幅被 clip 压缩 -68% |
| HDR f32 FWHM 更准确 | f32 fwhm_x mean=1.83 vs u16 fwhm_x mean=2.63 | 5s | 0 | PASS | 新 API FWHM 更小更准确, 旧 API 因 clip 导致 PSF 畸变 +44% |
| 业务源码范围合规 | git status 中 lib/plate_solve/ 与 lib/orchestrator/ 无改动 | 5s | 0 | PASS | 本任务仅修改 lib/dynamic_psf/, 未触及 P02-002 范围 |
| 旧 uint16 API 源码未修改 | diff dpsf_fit/dpsf_fit_batch/dpsf_free_results 实现 | 5s | 0 | PASS | 三个旧函数实现一字未改 (仅在文件末尾追加新函数) |

## Real-data metrics

- **测试数据**：testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts（4500×3600 uint16, Red 180s, T4 200mm f/2 wide FOV）。
- **星点检测**：numpy 局部最大值（5σ 阈值，min_distance=10），检测到 50 颗星，schema=star_det_v1:FLOAT64[N,6]，耗时 0.472s。
- **f32 API 性能**：47/50 有效拟合（94.0%），耗时 0.016s（含 OpenMP 16 线程并行）。
- **u16 API 性能**：47/50 有效拟合（94.0%），耗时 0.030s。
- **新 API 加速比**：0.030s → 0.016s（-47%），源于不创建全图 uint16→float 缓冲（4500×3600×4=64.8MB 节省）。
- **PSF 参数典型值**（f32 API, 47 颗均值）：B=1518.5（背景）, A=87578.0（振幅）, cx=2518.4, cy=896.7, sx=1.47, sy=1.56, fwhm_x=1.81, fwhm_y=1.93。FWHM ~1.9 像素符合 T4 200mm f/2 宽场光学系统典型值。
- **A/B 一致性**（原始 uint16 场景）：47 颗两者都成功的星点，9 字段（B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y）的 |diff|_mean=0.0, abs_rel_diff_max=0.0 — 完美一致，证明向后兼容。
- **A/B 差异性**（HDR 场景, scale=3.0）：48 颗两者都成功，A 的 abs_rel_diff_max=3.26（旧 API 振幅被 clip 压缩到 1/3），fwhm_x 的 abs_rel_diff_max=0.51（旧 API FWHM 高估 51%）— 证明新 API 在 HDR 场景下更准确。
- **失败帧分析**：3 颗星拟合失败（star[2]/star[3] 等），原因为背景约束违反（B/bkg0 ratio > 0.5），是拟合算法的正常拒绝（靠近图像边缘或密集星场），两个 API 都失败，行为一致。

## Failures and investigation

### 失败 1: star[2] (180, 245) 与 star[3] (3470, 250) 拟合失败

- **症状**：f32 API 与 u16 API 均返回 NaN（status != DPSF_FIT_OK）
- **根因**：日志显示 "Background constraint violated: B=265.64 bkg0=1759.50 ratio=0.8490"（star[2]）和 "B=376.72 bkg0=2169.00 ratio=0.8263"（star[3]）。Moffat4 拟合的背景 B 偏离初始估计 bkg0 超过 50%，被 dpsf_psf.cpp 第 337-342 行的背景约束检查拒绝。
- **影响**：3/50 颗失败（6%），属正常拟合拒绝率（密集星场或边缘星点）
- **处置**：不修复。这是 PSF 拟合算法的质量控制机制，两个 API 行为一致，证明新 API 未引入退化。
- **后续建议**：如需提高成功率，可放宽背景约束 ratio 阈值（当前 0.5）或增加 fitRadius（当前 8），但属 PSF 算法调优范围，超出本任务 API 增设目标。

### 退化 (非失败，记录为设计决策)

1. **PSF 参数 [N,9] 不含 status/mad/flux/eccentricity**：输出 9 字段为 B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y。失败的拟合以 NaN 标记，调用者通过 `out_n_valid` 和 `isnan()` 判断。如需 status/mad/flux/eccentricity，可扩展为 [N,13] 或调用旧的 dpsf_fit_batch（返回完整 DPSFFitResult 结构体）。这是为简化 Orchestrator 集成的设计决策。
2. **detections 列 2-5 未在拟合中使用**：新 API 仅使用 detections 的 [0]=x_px 和 [1]=y_px。[2]=flux、[3]=mag、[4]=saturated、[5]=has_saturated 由调用者填充但 PSF 拟合不消费。这是设计决策（PSF 拟合只需坐标），但 star_det v1 的完整 6 列仍需由生产者写入，以保证 block hash 完整性。
3. **detect_local_maxima 为测试用简化实现**：测试脚本中的星点检测用 numpy 局部最大值（非 star_detector），仅用于生成测试坐标。生产路径仍由 star_det v1 生产者（STAR_DETECT 或 PLATESOLVE_INTERNAL_EXPORT）提供坐标。
4. **未在全量 TestData 上回归**：本任务仅做单帧 A/B 验证。docs/05 §10 实施顺序要求 PSF 切换在第 7 步（P02-003 A/B 决策后），本任务为 API 能力准备。
