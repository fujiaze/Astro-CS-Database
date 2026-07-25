# REVIEW_REPORT

- Reviewer mode: 独立复核 (基于证据完整性 + 算法正确性 + 范围合规性 + 可复现性)
- Diff reviewed:
  - 修改 `lib/dynamic_psf/include/dynamic_psf.h` (3655 字节, SHA-256 E70159A1...) — 新增 dpsf_fit_batch_f32 声明 + 2 个 schema 宏 + doxygen 注释; 旧 3 个 API 声明未改
  - 修改 `lib/dynamic_psf/src/dpsf_psf.cpp` (23981 字节, SHA-256 7F8C6108...) — 新增 `<limits>` 头 + dpsf_fit_batch_f32 实现 (130 行); 旧 4 个函数 (dpsf_fit/dpsf_fit_batch/dpsf_free_results/moffat4_fit) 实现未改
  - 修改 `lib/dynamic_psf/python/dynamic_psf.py` (9708 字节, SHA-256 7FA3A79F...) — 新增 c_float 导入 + argtypes 注册 + fit_batch_f32 静态方法; 旧 fit/fit_batch 未改
  - 新增 `engineering/tools/psf_f32_ab_test.py` (15647 字节, SHA-256 ED55DC1E...) — A/B 测试脚本
  - 新增 `lib/dynamic_psf/dynamic_psf.dll` (334677 字节, SHA-256 A33286D6...) — 新构建 DLL
  - 新增 `build/artifacts/dynamic_psf.dll` (334677 字节, SHA-256 A33286D6...) — build/artifacts 副本
  - 新增 4 份 v1.1 报告 + psf_f32_impl.json
- Tests rerun:
  - 复查构建: mingw32-make clean + mingw32-make 成功, 仅 2 个旧代码无关警告 (maxIter/tolerance unused in dpsf_fit, 非本任务引入)
  - 复查导出符号: Python ctypes 确认 4 个符号全部可加载 (dpsf_fit/dpsf_fit_batch/dpsf_free_results/dpsf_fit_batch_f32)
  - 复查 DLL SHA-256: A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A (lib/dynamic_psf/ 与 build/artifacts/ 一致)
  - 复查旧 DLL 基线: 51AFFB4AD05737B3146274F8FDDB95208327DB336F4A645FF68C4D0032B43BA2 (构建前, 回滚参考)
  - 复查单帧 A/B (原始 uint16): 47/50 f32 有效, 47/50 u16 有效, 47 颗两者都成功, 9 字段 |diff|=0.000000 — 完美一致
  - 复查 HDR A/B (scale=3.0): 48/50 f32 有效, 48/50 u16 有效, A rel_diff_max=3.26, fwhm_x rel_diff_max=0.51 — 新 API 更准确
  - 复查 10 项有效性检查: 全部 PASS (A>0, sx>0.3, sy>0.3, fwhm_x>0, fwhm_y>0, fwhm_x<50, fwhm_y<50, B finite, cx in image, cy in image)
  - 复查旧 API 源码未修改: dpsf_fit (第 392-441 行) / dpsf_fit_batch (第 443-521 行) / dpsf_free_results (第 523-525 行) / moffat4_fit (第 212-390 行) 实现一字未改
  - 复查范围合规: lib/plate_solve/ 与 lib/orchestrator/ 无改动 (属 P02-002 范围)
- Contract/ABI/format findings:
  - **ABI 向后兼容**: 旧 3 个导出符号 (dpsf_fit/dpsf_fit_batch/dpsf_free_results) 的签名、参数、DPSFFitResult 结构体布局、行为完全未变。既有调用者无需重新编译即可继续使用新 DLL 的旧符号。
  - **新 API 签名**: `int dpsf_fit_batch_f32(const float*, int, int, const double*, int, const DPSFFitParams*, double*, int*)` — 直接接收 float32 图像 + star_det v1 FLOAT64 [N,6] detections, 输出 FLOAT64 [N,9] psf_params + int n_valid。
  - **新输出格式**: psf_params FLOAT64 [N,9], 9 字段 = B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y。失败行置 NaN。这是新格式, 不影响既有 DPSFFitResult 结构体 (13 字段)。
  - **schema 宏**: 新增 `DPSF_STAR_DET_SCHEMA_V1="star_det_v1:FLOAT64[N,6]"` 和 `DPSF_PSF_PARAMS_SCHEMA="psf_params:FLOAT64[N,9]"`, 用于日志记录与契约文档。
  - **Python 绑定**: 新增 `DynamicPSF.fit_batch_f32(image: np.float32, detections: np.float64 [N,6]) -> (np.float64 [N,9], int)` 静态方法; 既有 fit/fit_batch 方法未改。
- Scientific regression findings:
  - **无科学回归**: 旧 API 实现一字未改, 既有调用路径行为完全一致。
  - **A/B 一致性 (原始 uint16)**: 47 颗两者都成功的星点, 9 字段 |diff|_mean=0.0, abs_rel_diff_max=0.0 — 完美一致, 证明新 API 在 uint16 范围内数据上与旧 API 完全等价。
  - **A/B 差异性 (HDR)**: 新 API 在 HDR 场景下振幅测量更准确 (A=258985 vs 旧 API 82492, 旧 API 被 clip 压缩 -68%), FWHM 更精确 (fwhm_x=1.83 vs 旧 API 2.63, 旧 API 因 clip 畸变 +44%)。这是新 API 的设计目标 (不做 0-65535 clip)。
  - **PSF 参数典型值**: B=1518.5, A=87578.0, fwhm_x=1.81, fwhm_y=1.93 — FWHM ~1.9 像素符合 T4 200mm f/2 宽场光学系统典型值, 数值合理。
  - **失败率**: 3/50 (6%) 失败, 原因为背景约束违反 (B/bkg0 ratio > 0.5), 两个 API 都失败, 行为一致 — 属 PSF 拟合算法的正常质量控制, 非新 API 引入的退化。
- Risks:
  - **未在全量 TestData 上回归 (已知, 超出范围)**: 本任务仅做单帧 A/B 验证。docs/05 §10 实施顺序要求 PSF 切换在第 7 步 (P02-003 A/B 决策后)。本任务为 API 能力准备, 不切换生产链。后续 P02-007 或集成任务应在全量 710 帧上回归。
  - **star_det v1 生产者尚未确定 (路径 A vs B)**: docs/05 §4 要求 P02-003 全量 A/B 回归后选择路径。本任务的新 API 消费 star_det v1, 兼容两个路径, 但生产链切换需等路径决策。
  - **detections 列 2-5 未在拟合中使用 (设计决策)**: 新 API 仅使用 detections [0]=x_px 和 [1]=y_px。[2]-[5] 由调用者填充但 PSF 拟合不消费。Orchestrator 集成时需由调用者记录完整 star_det v1 block hash (含全部 6 列)。
  - **PSF 参数 [N,9] 不含 status/mad/flux/eccentricity (设计决策)**: 失败行以 NaN 标记。如需完整 13 字段, 可调用旧 dpsf_fit_batch (返回 DPSFFitResult 结构体)。后续可扩展为 [N,13] 或 [N,10]。
  - **detect_local_maxima 为测试用简化实现 (非生产)**: 测试脚本用 numpy 局部最大值生成坐标, 不替代生产路径的 sdet_detect_ex。生产路径由 star_det v1 生产者提供坐标。
  - **2 个构建警告 (旧代码, 非本任务引入)**: dpsf_fit 中 maxIter/tolerance 未使用, 属旧代码问题, 本任务未修复 (超出范围)。

## 详细复核

### 1. 任务目标达成度

| 目标 (来自 P02-005.md 与 docs/05 §8) | 达成情况 | 证据 |
|---|---|---|
| 新增 dpsf_fit_batch_f32, 接收 const float* | PASS | dynamic_psf.h 第 97-106 行, dpsf_psf.cpp 第 534-652 行 |
| 内部拟合使用 float/double | PASS | moffat4_fit 内部 lm_solve/moffat4_residual 全 double (未修改) |
| 不做 0-65535 clip | PASS | dpsf_psf.cpp 第 604-610 行直接从 float32 裁剪 patch, 无 clip 操作 |
| 旧 uint16 API 保留兼容 | PASS | 旧 3 个 API 实现一字未改 + A/B 测试 47/47 完全一致 |
| 消费 star_det v1 (FLOAT64 [N,6]) | PASS | detections 参数 const double* [N,6] |
| 不调用 sdet_detect_ex | PASS | 新 API 仅消费 detections 数组, 无 detector 调用 |
| 不创建整张 uint16 图像 | PASS | 仅裁剪 fitRadius×fitRadius 局部 patch |
| 记录消费的 schema/count | PASS | dpsf_psf.cpp 第 564-566 行日志记录 |
| 构建成功 | PASS | mingw32-make 成功, DLL 334677 字节, 4 个符号导出 |
| 单帧测试通过 | PASS | 47/50 有效, 10/10 有效性检查 PASS |
| float32 vs uint16 A/B | PASS | 原始场景 47/47 一致, HDR 场景新 API 更准确 |
| evidence/P02-005/ 下四份标准报告 | PASS | TASK_REPORT + TEST_REPORT + EVIDENCE_INDEX + REVIEW_REPORT + psf_f32_impl.json |

### 2. 范围合规性

- **仅修改 lib/dynamic_psf/**: ✅ 3 个源文件 (dynamic_psf.h / dpsf_psf.cpp / dynamic_psf.py)
- **未修改 lib/plate_solve/**: ✅ (属 P02-002 范围)
- **未修改 lib/orchestrator/**: ✅ (属 P02-002 范围)
- **旧 API 一字未改**: ✅ dpsf_fit / dpsf_fit_batch / dpsf_free_results / moffat4_fit 实现完全未变
- **不切换生产链**: ✅ 本任务为 API 能力准备, 不修改 Orchestrator 调用路径

### 3. 算法正确性

- **moffat4_fit 复用**: ✅ 新 API 内部调用既有 moffat4_fit (float* 输入 + double 拟合), 算法与旧 API 完全一致
- **patch 裁剪逻辑**: ✅ 与旧 dpsf_fit_batch 的 rect 计算逻辑一致 (x0=max(0,cx-fitRadius), x1=min(width,cx+fitRadius+1))
- **坐标转换**: ✅ 局部坐标 → 图像坐标 (result.cx += x0, result.cy += y0), 与旧 API 一致
- **OpenMP 并行**: ✅ `#pragma omp parallel for schedule(dynamic) reduction(+:success_count)`, 与旧 dpsf_fit_batch 一致
- **失败处理**: ✅ 失败行置 NaN (新), 旧 API 失败行 status=非 OK 但字段为 0; A/B 对比时两者都标记为失败, 不影响一致性
- **PSF 参数典型值**: ✅ FWHM ~1.9 像素, B ~1518, A ~87578 — 数值合理

### 4. 可复现性

- **构建可复现**: ✅ mingw32-make clean + mingw32-make, 输入源码不变则 DLL SHA-256 一致
- **A/B 测试可复现**: ✅ python engineering/tools/psf_f32_ab_test.py, 输入 FITS + DLL 不变则结果一致 (modulo OpenMP 调度抖动)
- **HDR 测试可复现**: ✅ 内联 Python 脚本, scale=3.0 + offset=500 固定
- **DLL 副本一致**: ✅ lib/dynamic_psf/dynamic_psf.dll 与 build/artifacts/dynamic_psf.dll SHA-256 完全一致

### 5. 证据完整性

- **SHA-256 全部采集**: ✅ 12 个主要文件 SHA-256 已记录在 EVIDENCE_INDEX.md
- **旧 DLL 基线 SHA-256**: ✅ 51AFFB4A... (构建前, 回滚参考)
- **新 DLL SHA-256**: ✅ A33286D6... (构建后, lib/ 与 build/artifacts/ 一致)
- **测试结果 JSON**: ✅ psf_f32_ab_result.json (原始场景) + psf_f32_hdr_result.json (HDR 场景)
- **导出符号验证**: ✅ Python ctypes 确认 4 个符号可加载

## VERDICT: PASS

### 通过理由

1. **任务目标全部达成**: docs/05 §8 全部 8 项要求满足, P02-005.md 全部执行步骤完成
2. **ABI 向后兼容**: 旧 3 个 API 实现一字未改, A/B 测试 47/47 完美一致 (9 字段 |diff|=0)
3. **新 API 正确性**: 10/10 有效性检查 PASS, PSF 参数数值合理 (FWHM ~1.9 像素)
4. **新 API 优势**: HDR 场景下振幅测量 +68%, FWHM 精度 +30% (不做 clip 的设计目标达成)
5. **构建成功**: mingw32-make 成功, 4 个符号导出, DLL SHA-256 采集
6. **范围合规**: 仅修改 lib/dynamic_psf/, 未触及 P02-002 范围
7. **证据完整**: 12 个文件 SHA-256 采集, 4 份 v1.1 报告 + psf_f32_impl.json 生成

### 后续建议 (非阻塞)

1. P02-003 A/B 决策后, 在全量 710 帧 TestData 上回归新 API (P02-007 或集成任务)
2. Orchestrator 集成时, 由调用者记录完整 star_det v1 block hash (含全部 6 列)
3. 如需完整 PSF 诊断信息 (status/mad/flux/eccentricity), 可扩展输出为 [N,13] 或调用旧 dpsf_fit_batch
4. 修复旧代码的 2 个警告 (dpsf_fit 中 maxIter/tolerance 未使用), 属独立 PR
5. 考虑在 dpsf_fit_batch_f32 中增加 block hash 参数 (可选), 让 PSF 模块自己记录消费的 block hash
