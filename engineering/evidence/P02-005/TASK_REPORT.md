# TASK_REPORT

- Task ID: P02-005（Dynamic PSF float32 API, v1.1 开发包）
- Commit/base: HEAD = 7b85ff3f0d37a4b26fff6077684993842ed2bbae（"P01-002: 建立依赖锁定清单"）；远端 origin = https://github.com/fujiaze/Astro-CS-Database.git；包版本 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- Objective: 新增 `dpsf_fit_batch_f32` API，直接接收 `const float* image`。内部拟合使用 float/double，不做 0–65535 clip。旧 uint16 API 保留兼容。消费 star_det v1（FLOAT64 [N,6]），不调用 sdet_detect_ex，不创建整张 uint16 图像。覆盖 docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md §8 全部要求。
- Changes:
  - **lib/dynamic_psf/include/dynamic_psf.h**：新增 `dpsf_fit_batch_f32` C API 声明 + 两个 schema 宏（`DPSF_STAR_DET_SCHEMA_V1`、`DPSF_PSF_PARAMS_SCHEMA`）+ 完整 doxygen 风格注释（输入/输出/返回值/字段定义）。旧 `dpsf_fit` / `dpsf_fit_batch` / `dpsf_free_results` 声明一字未改。
  - **lib/dynamic_psf/src/dpsf_psf.cpp**：新增 `<limits>` 头文件（用于 `std::numeric_limits<double>::quiet_NaN()`）；在文件末尾新增 `dpsf_fit_batch_f32` 实现（约 130 行）。实现要点：(1) 直接接收 `const float* image`，不做 uint16 转换、不做 0-65535 clip；(2) 从 `detections` FLOAT64 [N,6] 提取 cx/cy（第 0/1 列），其余列（flux/mag/saturated/has_saturated）由调用者填充，PSF 拟合不依赖；(3) 每颗星仅裁剪局部 patch（fitRadius×fitRadius 级别），不创建整张 uint16 缓冲；(4) 内部调用既有 `moffat4_fit`（已经是 float* 输入 + double 拟合），保持算法一致性；(5) OpenMP 并行 `schedule(dynamic)`；(6) 失败拟合的 9 字段全部置 NaN；(7) 日志记录消费的 schema 与 count（P02-005 §8 要求）；(8) 既有 `dpsf_fit` / `dpsf_fit_batch` / `dpsf_free_results` / `moffat4_fit` 实现一字未改。
  - **lib/dynamic_psf/python/dynamic_psf.py**：新增 `c_float` 导入；在 `_load_dll` 中注册 `dpsf_fit_batch_f32` 的 argtypes/restype；在 `DynamicPSF` 类新增 `fit_batch_f32` 静态方法（约 65 行），接收 `np.float32` 图像 + `np.float64` detections，返回 `(psf_params[N,9], n_valid)`。既有 `fit` / `fit_batch` 方法一字未改。
  - **engineering/tools/psf_f32_ab_test.py**：新增 A/B 测试脚本（约 400 行），读取真实 FITS 帧 → numpy 局部最大值检测生成 star_det v1 [N,6] → 调用 f32 API + u16 API → 逐星对比 9 字段 + 10 项有效性检查。
  - **build/artifacts/dynamic_psf.dll**：新构建产物（334677 字节），导出 4 个符号（dpsf_fit / dpsf_fit_batch / dpsf_free_results / dpsf_fit_batch_f32）。
- Files:
  - `lib/dynamic_psf/include/dynamic_psf.h`（修改，3655 字节，SHA-256 E70159A1317BDDBAB9257EA3436638338817D9B9C2032B5EBD2B1C02B02AEE8F）
  - `lib/dynamic_psf/src/dpsf_psf.cpp`（修改，23981 字节，SHA-256 7F8C61083F7160B654AD4FB47AD2FEFE2D11023F8596ED5908ECAF9C1408ED2C）
  - `lib/dynamic_psf/python/dynamic_psf.py`（修改，9708 字节，SHA-256 7FA3A79F871FF6DEDBEE759401BB8C8DC9FA9FCA94EBFE95528332C1D559F952）
  - `lib/dynamic_psf/dynamic_psf.dll`（新构建，334677 字节，SHA-256 A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A）
  - `build/artifacts/dynamic_psf.dll`（新构建副本，334677 字节，SHA-256 A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A，与源一致）
  - `engineering/tools/psf_f32_ab_test.py`（新增，15647 字节，SHA-256 ED55DC1E8657D6D3584D08DF1C202077762E7FB7068B9D22DC9F384F63B2D377）
  - `engineering/evidence/P02-005/psf_f32_ab_result.json`（测试结果，4844 字节，SHA-256 2FB8E52771544685B6D502499501B41B35EB512FF79B912992BD311196683033）
  - `engineering/evidence/P02-005/psf_f32_hdr_result.json`（HDR 测试结果，268 字节，SHA-256 A3CE468B89D3ECA16CA3BCA2F74797E28DC65D9B26FAA1173A2E1339F3DA1C35）
  - `engineering/evidence/P02-005/TASK_REPORT.md`（本文件）
  - `engineering/evidence/P02-005/TEST_REPORT.md`（测试报告）
  - `engineering/evidence/P02-005/EVIDENCE_INDEX.md`（证据索引）
  - `engineering/evidence/P02-005/REVIEW_REPORT.md`（独立复核报告）
  - `engineering/evidence/P02-005/psf_f32_impl.json`（结构化实现摘要）
- Compatibility:
  - **ABI 向后兼容**：旧 `dpsf_fit` / `dpsf_fit_batch` / `dpsf_free_results` 三个导出符号的签名、参数、结构体布局、行为完全未变；既有调用者（如 orchestrator stage1 路径）无需重新编译即可继续使用旧 DLL 的等价符号（但建议升级到新 DLL 以获得 dpsf_fit_batch_f32）。
  - **Python 绑定向后兼容**：`DynamicPSF.fit` / `DynamicPSF.fit_batch` 方法签名与行为一字未改；新增 `fit_batch_f32` 为独立静态方法。
  - **数据格式兼容**：新 API 消费 star_det v1（FLOAT64 [N,6]），与 docs/05 §5 定义的统一格式一致；输出 psf_params FLOAT64 [N,9] 是新格式，不影响既有 DPSFFitResult 结构体。
  - **真实数据 A/B 验证**：在 Galaxy_Center Red 180s 真实帧上，新 f32 API 与旧 u16 API 在 47/50 颗星点上输出完全一致（所有 9 字段 |diff|=0.0），证明在 uint16 范围内数据上完全向后兼容。
- Rollback:
  - 回滚方法 1（推荐）：从 git 历史恢复 `lib/dynamic_psf/include/dynamic_psf.h`、`lib/dynamic_psf/src/dpsf_psf.cpp`、`lib/dynamic_psf/python/dynamic_psf.py` 三个文件，然后重新 `mingw32-make clean && mingw32-make` 构建 DLL。
  - 回滚方法 2（快速）：删除新增的 `dpsf_fit_batch_f32` 声明与实现（位于 dynamic_psf.h 第 56-106 行、dpsf_psf.cpp 第 527-653 行、dynamic_psf.py 第 130-141 行 + 第 219-282 行），保留旧 API。
  - 回滚方法 3（仅 DLL）：用旧 DLL（SHA-256 51AFFB4AD05737B3146274F8FDDB95208327DB336F4A645FF68C4D0032B43BA2）覆盖 build/artifacts/dynamic_psf.dll 即可恢复旧 ABI（但源码仍含新 API 声明，需注意不要调用）。
  - 删除 `engineering/tools/psf_f32_ab_test.py` 与 `engineering/evidence/P02-005/` 即可回滚测试产物。
  - 不影响 lib/plate_solve/ 或 lib/orchestrator/（本任务未修改这两个模块）。
- Remaining risks:
  - **未在全量 TestData 上回归（已知，超出范围）**：本任务仅做单帧 A/B 验证（Galaxy_Center Red 180s），未在 710 帧全量 TestData 上回归。docs/05 §10 实施顺序要求"PSF 切换 float32 API 并消费选定路径产生的 star_det"在第 7 步，需在 P02-003 A/B 决策后进行；本任务为 API 能力准备，不切换生产链。
  - **star_det v1 生产者尚未确定（路径 A vs B）**：docs/05 §4 决策规则要求 P02-003 全量 A/B 回归后选择路径 A 或 B。本任务的 `dpsf_fit_batch_f32` 消费 star_det v1，但不关心生产者是 STAR_DETECT 还是 PLATESOLVE_INTERNAL_EXPORT，两个路径都兼容。
  - **detections 列 2-5 未在拟合中使用**：新 API 仅使用 detections 的 [0]=x_px 和 [1]=y_px 进行拟合，[2]=flux、[3]=mag、[4]=saturated、[5]=has_saturated 由调用者填充但 PSF 拟合不消费。这是设计决策（PSF 拟合只需坐标），但应在 Orchestrator 集成时由调用者记录完整的 star_det v1 block hash（含全部 6 列）。
  - **PSF 参数 [N,9] 不含 status/mad/flux/eccentricity**：输出 9 字段为 B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y，未包含 status、mad、flux、eccentricity。失败的拟合以 NaN 标记，调用者通过 `out_n_valid` 和 `isnan()` 判断。如需 status，可扩展为 [N,10] 或单独 status 数组（后续任务）。
  - **detect_local_maxima 为测试用简化实现**：测试脚本中的星点检测用 numpy 局部最大值（非 star_detector），仅用于生成测试坐标，不替代生产路径的 sdet_detect_ex。生产路径仍由 star_det v1 生产者提供坐标。

## 详细执行结果

### 1. 现有 API 分析（执行步骤 1）

| 项 | 值 |
|---|---|
| 现有 uint16 API | `dpsf_fit`（单星）、`dpsf_fit_batch`（批量）、`dpsf_free_results`（释放） |
| 现有批量签名 | `int dpsf_fit_batch(const uint16_t* image, int w, int h, const double* cx_array, const double* cy_array, int count, const DPSFFitParams* params, DPSFFitResult** out_results)` |
| 现有输入格式 | uint16 图像 + 两个独立 cx/cy 数组（非 star_det v1 [N,6]） |
| 现有内部数据流 | uint16 图像 → 全图转 float_image（width×height float 缓冲）→ 每星裁剪 patch → moffat4_fit（float* + double 拟合） |
| 现有 clip 位置 | Python 端 `np.clip(image, 0, 65535).astype(np.uint16)`（dynamic_psf.py 第 158/181 行）；C++ 端 dpsf_fit_batch 不 clip（仅 uint16→float 转换） |
| moffat4_fit 内部 | 已经是 `const float* image` 输入 + double 拟合（lm_solve / moffat4_residual 全 double） |
| 关键发现 | **核心拟合逻辑已经是 float/double，无需修改 moffat4_fit**；只需新增一个不创建全图 uint16 缓冲、直接消费 star_det v1 的入口 |

### 2. 新 API 设计（执行步骤 2）

| 项 | 值 |
|---|---|
| 新 API 名 | `dpsf_fit_batch_f32` |
| 输入图像 | `const float* image`（float32，行主序） |
| 输入检测 | `const double* detections`（star_det v1 FLOAT64 [N,6]） |
| 输出参数 | `double* out_psf_params`（FLOAT64 [N,9]，调用者分配） |
| 输出有效数 | `int* out_n_valid`（成功拟合数） |
| 返回值 | 0=成功完成批量；-1=参数错误 |
| 9 字段定义 | [0]=B [1]=A [2]=cx [3]=cy [4]=sx [5]=sy [6]=theta [7]=fwhm_x [8]=fwhm_y |
| 失败行标记 | 全部 9 字段置 NaN |
| schema 宏 | `DPSF_STAR_DET_SCHEMA_V1="star_det_v1:FLOAT64[N,6]"`、`DPSF_PSF_PARAMS_SCHEMA="psf_params:FLOAT64[N,9]"` |
| 默认参数 | params 可为 NULL，使用 fitRadius=8/maxIter=200/tolerance=1e-8 |
| 并行 | OpenMP `#pragma omp parallel for schedule(dynamic) reduction(+:success_count)` |
| 全图 uint16 缓冲 | **不创建**（仅裁剪 fitRadius×fitRadius 局部 patch） |
| 0-65535 clip | **不做**（直接使用 float32 输入） |
| sdet_detect_ex | **不调用**（仅消费 detections 数组） |

### 3. 构建结果（执行步骤 3）

| 项 | 值 |
|---|---|
| 构建工具 | mingw32-make 4.4.1 + g++ 16.1.0 (MSYS2 mingw64) |
| 构建命令 | `mingw32-make clean; mingw32-make`（在 lib/dynamic_psf/） |
| 构建结果 | 成功，仅 2 个无关警告（dpsf_fit 中 maxIter/tolerance 未使用，属旧代码问题） |
| DLL 大小 | 334677 字节 |
| DLL SHA-256 | A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A |
| 导出符号验证 | Python ctypes 确认 4 个符号全部可加载：dpsf_fit / dpsf_fit_batch / dpsf_free_results / dpsf_fit_batch_f32 |
| build/artifacts 副本 | SHA-256 与源一致（A33286D6...） |

### 4. 单帧 A/B 验证（执行步骤 4）

#### 4.1 原始 uint16 数据场景（Galaxy_Center Red 180s）

| 指标 | f32 API（新） | u16 API（旧） |
|---|---|---|
| 图像尺寸 | 4500×3600 | 4500×3600 |
| 原始 dtype | uint16 | uint16 |
| 像素范围 | [1267, 65535] | [1267, 65535] |
| 检测星点数 | 50 | 50 |
| 有效拟合 | 47/50 (94.0%) | 47/50 (94.0%) |
| 耗时 | 0.016s | 0.030s |
| 两者都成功 | 47 | 47 |
| 9 字段 \|diff\|_max | 0.000000 | 0.000000 |
| 有效性检查 | 10/10 PASS | 10/10 PASS |

**结论**：在 uint16 范围内数据上，新 f32 API 与旧 u16 API 输出完全一致（所有 9 字段差异为 0），证明向后兼容。新 API 耗时更低（0.016s vs 0.030s，-47%），因为不创建全图 uint16→float 缓冲。

#### 4.2 高动态范围（HDR）模拟场景（scale=3.0, offset=500）

| 指标 | f32 API（新） | u16 API（旧） |
|---|---|---|
| HDR 范围 | [4301, 197105] | [4301, 65535]（clip） |
| 超 65535 像素 | 7295 (0.05%) | 全部 clip 到 65535 |
| 有效拟合 | 48/50 (96.0%) | 48/50 (96.0%) |
| 耗时 | 0.012s | 0.026s |
| A (振幅) mean | 258985 | 82492（被 clip 压缩，-68%） |
| fwhm_x mean | 1.83 | 2.63（clip 导致 PSF 畸变，+44%） |
| fwhm_y mean | 1.88 | 2.60（clip 导致 PSF 畸变，+38%） |
| A rel_diff_max | 3.26（旧 API 严重低估振幅） | - |
| fwhm_x rel_diff_max | 0.51（旧 API 高估 FWHM） | - |

**结论**：在 HDR 场景下，旧 API 因 clip 丢失高亮像素信息，导致振幅被严重低估（-68%）、FWHM 被高估（+44%）；新 f32 API 保留完整动态范围，输出更准确的 PSF 参数。这正是 docs/05 §8 "不做 0–65535 clip" 的设计目标。

### 5. 与 docs/05 §8 要求对照

| §8 要求 | 实现 | 证据 |
|---|---|---|
| 新增 `dpsf_fit_batch_f32`，直接接收 `const float* image` | ✅ dynamic_psf.h 第 97-106 行 | TASK_REPORT §2 |
| 内部拟合使用 float/double | ✅ moffat4_fit 内部 lm_solve/moffat4_residual 全 double | dpsf_psf.cpp 第 26-179 行（未修改） |
| 不做 0–65535 clip | ✅ 新 API 不调用 np.clip / 不做 uint16 转换 | dpsf_psf.cpp 第 604-610 行（直接从 float32 裁剪 patch） |
| 旧 uint16 API 保留兼容 | ✅ dpsf_fit / dpsf_fit_batch / dpsf_free_results 一字未改 | A/B 测试 47/47 完全一致 |
| 消费同一个 star_det v1（FLOAT64 [N,6]） | ✅ detections 参数为 const double* [N,6] | dynamic_psf.h 第 100 行 |
| 不调用 sdet_detect_ex | ✅ 新 API 仅消费 detections 数组，不调用任何 detector | dpsf_psf.cpp 第 534-652 行 |
| 不创建整张 uint16 图像 | ✅ 仅裁剪 fitRadius×fitRadius 局部 patch | dpsf_psf.cpp 第 604-610 行 |
| 记录消费的 block hash、count 和 schema | ✅ 日志记录 schema 与 count（block hash 由调用者记录） | dpsf_psf.cpp 第 564-566 行 |
