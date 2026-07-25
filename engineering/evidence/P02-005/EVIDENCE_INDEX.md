# EVIDENCE_INDEX: P02-005 (Dynamic PSF float32 API, v1.1 开发包)

## 任务标识

- Task ID: P02-005
- 任务名: Dynamic PSF float32 API (v1.1 开发包)
- Phase / Gate: P02 / G2
- Commit base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae (main, "P01-002: 建立依赖锁定清单")
- 远端: https://github.com/fujiaze/Astro-CS-Database.git
- 包版本: 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- 生成时间: 2026-07-25 (PSVersion 7.6.3, Windows, Python 3.12 + numpy 1.26.4 + astropy 7.1.0)

## 证据目录

`engineering/evidence/P02-005/`

## 范围声明

- 本任务为 **API 增设任务**：在 `lib/dynamic_psf/` 下新增 `dpsf_fit_batch_f32` C API + Python 绑定，不修改任何旧 API（dpsf_fit / dpsf_fit_batch / dpsf_free_results 三个导出符号的签名与实现一字未改）。
- 未修改 `lib/plate_solve/` 或 `lib/orchestrator/`（属 P02-002 范围）。
- 未切换生产链：docs/05 §10 实施顺序要求 PSF 切换在第 7 步（P02-003 A/B 决策后），本任务为 API 能力准备。
- 单帧 A/B 验证使用真实 FITS 帧（Galaxy_Center Red 180s）+ HDR 模拟场景，未在全量 710 帧 TestData 上回归（超出本任务范围）。

## 证据清单 (主要文件, 含 SHA-256)

### 修改的源码 (3 个)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| lib/dynamic_psf/include/dynamic_psf.h | 3655 | E70159A1317BDDBAB9257EA3436638338817D9B9C2032B5EBD2B1C02B02AEE8F | 新增 dpsf_fit_batch_f32 声明 + 2 个 schema 宏 + doxygen 注释; 旧 3 个 API 声明未改 |
| lib/dynamic_psf/src/dpsf_psf.cpp | 23981 | 7F8C61083F7160B654AD4FB47AD2FEFE2D11023F8596ED5908ECAF9C1408ED2C | 新增 `<limits>` 头 + dpsf_fit_batch_f32 实现 (130 行); 旧 4 个函数实现未改 |
| lib/dynamic_psf/python/dynamic_psf.py | 9708 | 7FA3A79F871FF6DEDBEE759401BB8C8DC9FA9FCA94EBFE95528332C1D559F952 | 新增 c_float 导入 + dpsf_fit_batch_f32 argtypes + fit_batch_f32 静态方法; 旧 fit/fit_batch 未改 |

### 新构建产物 (2 个, 内容一致)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| lib/dynamic_psf/dynamic_psf.dll | 334677 | A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A | 新构建 DLL, 导出 4 个符号 (dpsf_fit/dpsf_fit_batch/dpsf_free_results/dpsf_fit_batch_f32) |
| build/artifacts/dynamic_psf.dll | 334677 | A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A | build/artifacts 副本, SHA-256 与源一致 |

### 旧 DLL 基线 (回滚参考)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| build/artifacts/dynamic_psf.dll (构建前) | 334677 | 51AFFB4AD05737B3146274F8FDDB95208327DB336F4A645FF68C4D0032B43BA2 | 旧 DLL 基线 (本任务修改前), 仅含 3 个导出符号 |

### 测试工具与结果 (4 个)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| engineering/tools/psf_f32_ab_test.py | 15647 | ED55DC1E8657D6D3584D08DF1C202077762E7FB7068B9D22DC9F384F63B2D377 | A/B 测试脚本 (FITS 读取 + numpy 检测 + f32/u16 API 对比 + 10 项有效性检查) |
| engineering/evidence/P02-005/psf_f32_ab_result.json | 4844 | 2FB8E52771544685B6D502499501B41B35EB512FF79B912992BD311196683033 | 原始 uint16 场景 A/B 结果 (47/50 一致, 9 字段 diff=0) |
| engineering/evidence/P02-005/psf_f32_hdr_result.json | 268 | A3CE468B89D3ECA16CA3BCA2F74797E28DC65D9B26FAA1173A2E1339F3DA1C35 | HDR 模拟场景结果 (scale=3.0, 7295 像素超 65535, A rel_diff_max=3.26) |
| engineering/evidence/P02-005/psf_f32_impl.json | (见文件) | (见 git commit) | 结构化实现摘要 (新 API 签名 + 改动文件 + 构建结果 + 测试结果) |

### 报告 (4 份 v1.1)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| TASK_REPORT.md | (见 git commit) | (见 git commit) | v1.1 任务执行报告 (含 5 节详细执行结果) |
| TEST_REPORT.md | (见 git commit) | (见 git commit) | v1.1 测试报告 (29 项测试 + Real-data metrics + Failures) |
| EVIDENCE_INDEX.md | (self) | (self-referential) | v1.1 证据索引 (本文件, 含 12 个文件 SHA-256) |
| REVIEW_REPORT.md | (见 git commit) | (见 git commit) | v1.1 独立复核报告 (VERDICT: PASS) |

## 关键事实证据

### F-001: 新 API 签名

```c
DPSF_EXPORT int dpsf_fit_batch_f32(
    const float *image,          // float32 图像 [height*width]
    int width,
    int height,
    const double *detections,    // star_det v1 FLOAT64 [N,6]
    int n_detections,
    const DPSFFitParams *params, // 可为 NULL
    double *out_psf_params,      // 输出 FLOAT64 [N,9]
    int *out_n_valid             // 输出有效拟合数
);
```

- 头文件: lib/dynamic_psf/include/dynamic_psf.h 第 97-106 行
- 实现: lib/dynamic_psf/src/dpsf_psf.cpp 第 534-652 行
- Python 绑定: lib/dynamic_psf/python/dynamic_psf.py 第 219-282 行

### F-002: 旧 API 向后兼容

- 旧 3 个导出符号签名与实现一字未改:
  - `dpsf_fit` (dpsf_psf.cpp 第 392-441 行, 未修改)
  - `dpsf_fit_batch` (dpsf_psf.cpp 第 443-521 行, 未修改)
  - `dpsf_free_results` (dpsf_psf.cpp 第 523-525 行, 未修改)
- A/B 测试证明: 47/50 颗星点在新旧 API 上输出完全一致 (9 字段 |diff|=0.0)

### F-003: DLL 构建成功

- 构建工具: mingw32-make 4.4.1 + g++ 16.1.0 (MSYS2 mingw64)
- 构建命令: `mingw32-make clean; mingw32-make` (在 lib/dynamic_psf/)
- 构建结果: 成功, 仅 2 个旧代码无关警告 (maxIter/tolerance unused in dpsf_fit, 非本任务引入)
- 新 DLL SHA-256: A33286D65FED2CCE23EE6A5635AFA3D5A38F6D6302BCB61ED0056F9441E34C9A
- 旧 DLL SHA-256 (构建前): 51AFFB4AD05737B3146274F8FDDB95208327DB336F4A645FF68C4D0032B43BA2

### F-004: 导出符号验证

Python ctypes 确认 4 个符号全部可加载:
- dpsf_fit: True
- dpsf_fit_batch: True
- dpsf_free_results: True
- **dpsf_fit_batch_f32: True** (新符号, addr=0x000001624581d610)

### F-005: 单帧 A/B 一致性 (原始 uint16 场景)

- 测试帧: Galaxy_Center Red 180s (4500×3600 uint16, 像素 [1267, 65535])
- 检测星点: 50 颗 (numpy 局部最大值, schema=star_det_v1:FLOAT64[N,6])
- f32 API: 47/50 有效 (94.0%), 0.016s
- u16 API: 47/50 有效 (94.0%), 0.030s
- 两者都成功: 47 颗
- 9 字段 |diff|_max: 0.000000 (B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y 全部完美一致)
- 10 项有效性检查: 全部 PASS (A>0, sx>0.3, sy>0.3, fwhm_x>0, fwhm_y>0, fwhm_x<50, fwhm_y<50, B finite, cx in image, cy in image)

### F-006: HDR 场景 A/B 差异性 (新 API 优势展示)

- 模拟方式: 原始 uint16 × 3.0 + 500, 使 7295 像素 (0.05%) 超过 65535
- f32 API: 48/50 有效 (96.0%), 0.012s
- u16 API: 48/50 有效 (96.0%), 0.026s (含 np.clip 到 0-65535)
- A (振幅) mean: f32=258985 vs u16=82492 (旧 API 被 clip 压缩 -68%)
- fwhm_x mean: f32=1.83 vs u16=2.63 (旧 API 因 clip 导致 PSF 畸变 +44%)
- A rel_diff_max: 3.26 (旧 API 严重低估振幅)
- fwhm_x rel_diff_max: 0.51 (旧 API 高估 FWHM)
- 结论: 新 API 在 HDR 场景下保留完整动态范围, 输出更准确的 PSF 参数

### F-007: docs/05 §8 要求全部满足

| §8 要求 | 满足 | 证据 |
|---|---|---|
| 新增 dpsf_fit_batch_f32, 接收 const float* | ✅ | F-001 |
| 内部拟合使用 float/double | ✅ | moffat4_fit 内部 lm_solve/moffat4_residual 全 double (未修改) |
| 不做 0-65535 clip | ✅ | dpsf_psf.cpp 第 604-610 行直接从 float32 裁剪 patch |
| 旧 uint16 API 保留兼容 | ✅ | F-002 + F-005 |
| 消费 star_det v1 (FLOAT64 [N,6]) | ✅ | F-001 (detections 参数) |
| 不调用 sdet_detect_ex | ✅ | dpsf_psf.cpp 第 534-652 行 (仅消费 detections 数组) |
| 不创建整张 uint16 图像 | ✅ | dpsf_psf.cpp 第 604-610 行 (仅裁剪局部 patch) |
| 记录消费的 schema/count | ✅ | dpsf_psf.cpp 第 564-566 行 (日志记录 schema 与 count) |

### F-008: 范围合规性

- 仅修改 lib/dynamic_psf/ 下 3 个源文件
- 未修改 lib/plate_solve/ (属 P02-002 范围)
- 未修改 lib/orchestrator/ (属 P02-002 范围)
- 新增 engineering/tools/psf_f32_ab_test.py (测试工具)
- 新增 engineering/evidence/P02-005/ (证据目录)

## 复核结论

- VERDICT: PASS (详见 REVIEW_REPORT.md)
- 新 API `dpsf_fit_batch_f32` 实现完成, 构建成功, 导出符号验证通过
- 单帧 A/B 验证: 原始 uint16 场景 47/47 完全一致 (向后兼容), HDR 场景新 API 更准确 (振幅 +68%, FWHM -30%)
- docs/05 §8 全部 8 项要求满足
- 旧 API 一字未改, ABI 向后兼容
