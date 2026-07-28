# P12-004 任务报告：T1-T4 与滤镜类别测光矩阵验证

- **任务编号**: P12-004
- **Gate**: G12 (Photometric Diagnostic)
- **开始时间**: 2026-07-28
- **完成时间**: 2026-07-28
- **状态**: DONE (CONDITIONAL — 全部 16 帧 Gate 失败，需进入 P12-005 修复)
- **依赖**: P12-002 (DONE), P12-003 (DONE), P10-006 (DONE)
- **参考**: `docs/06_PHOTOMETRY_CORRECTION_SPEC.md`, P10-006 代表帧清单
- **后续**: P12-005 (修复 SNR 模型与 HISS 持久化)

## 1. 任务目标

在 P12-002（KD-tree 方向 bug 修复 + 双向最近邻唯一配对）和 P12-003（光谱积分与响应曲线无回归验证）完成后，本任务目标为：

1. 对 16 帧代表帧（T2/T3/T4 × LUM/RED/GREEN/BLUE/HA/OIII）运行 orchestrator stage1 测光校准
2. 收集 PhotometricDiag 诊断字段（20 字段）
3. 生成 PHOTOMETRY_MATRIX.csv 测光矩阵
4. 检查 Gate（Broadband/LRGB fit_used ≥ 20，窄带 fit_used ≥ 8；scale_factor ∈ [0.01, 100.0]；sigma_residual > 0）
5. 对失败帧进行分类（INSUFFICIENT_STARS / ZERO_SIGMA / INVALID_SCALE / STAGE1_ERROR / TIMEOUT）

## 2. 完成内容

### 2.1 测试脚本编写

编写 `scripts/run_photometric_matrix.py`（26166 bytes），实现：

- **16 帧代表帧清单**：T4 (5 帧, Galaxy_Center) + T2 (5 帧, LDN43/NGC1727) + T3 (6 帧, NGC55)
- **单帧测光校准**：调用 `orchestrator.exe stage1`，参数包括 `--frame`, `--output`, `--gaia-data`, `--filter`, `--config`, `--log-level`
- **诊断信息收集**：解析 stdout JSON 的 `photo_stats` 字段（17 字段）+ 读取 `photometry_report.json`（20 字段）
- **Gate 检查**：按滤镜类别（Broadband fit_used ≥ 20，Narrowband fit_used ≥ 8）+ scale_factor 范围 + sigma_residual 有限性
- **失败分类**：5 类失败原因（INSUFFICIENT_STARS / ZERO_SIGMA / INVALID_SCALE / STAGE1_ERROR / TIMEOUT）
- **报告生成**：PHOTOMETRY_MATRIX.csv + photometric_diag_summary.json + failure_classification.json

### 2.2 16 帧测光矩阵执行

对 16 帧代表帧逐帧运行 orchestrator stage1（每帧超时 600s）：

- **总运行时间**: ~178s（含成功帧与失败帧）
- **成功运行 stage1 (exit_code=0)**: 3 帧（T4 RED/GREEN/BLUE）
- **stage1 失败 (exit_code≠0)**: 13 帧
- **Gate PASS**: 0/16
- **Gate FAIL**: 16/16

### 2.3 失败帧分类

#### 2.3.1 INVALID_SCALE (3 帧)

T4 Galaxy_Center 的 RED/GREEN/BLUE 三帧 stage1 成功执行，但 scale_factor ≈ 0.0026-0.0028，远低于 Gate 下限 0.01：

| 帧 | fit_used | scale_factor | sigma_residual | n_matched |
|----|----------|--------------|----------------|-----------|
| T4 RED | 1670 | 0.002836 | 0.181595 | 1670 |
| T4 GREEN | 1619 | 0.002696 | 0.157614 | 1619 |
| T4 BLUE | 1231 | 0.00261 | 0.128533 | 1231 |

诊断字段完整填充（valid_fsyn=0 表明光谱合成异常，可能是 scale_factor 过小的根因）。

#### 2.3.2 STAGE1_ERROR — 滤光片曲线加载失败 (2 帧)

T4 Galaxy_Center 的 HA/OIII 两帧在 PHOTOMETRIC 阶段失败：

```
error_msg: "[PHOTOMETRIC] 加载滤光片曲线失败"
exit_code: 1
```

**根因**: orchestrator 的 `map_filter_name` 函数未正确映射 "H-alpha" 和 "OIII" 到 `filters.json` 中的滤光片名称，且 `filters.json` 中缺少窄带滤光片定义。

#### 2.3.3 STAGE1_ERROR — 中文路径导致文件系统错误 (4 帧)

T2 LDN43 的 RED/GREEN/BLUE/HA 四帧在读取 FITS 文件时失败：

```
what():  filesystem error: Cannot convert character sequence: Illegal byte sequence
exit_code: 3
```

**根因**: T2 帧的文件路径中包含中文（`LDN43_T2素材_flying_dutchman`），C++ `std::filesystem` 在 Windows 下无法处理非 ASCII 路径。

#### 2.3.4 STAGE1_ERROR — 无 Master 文件且未启用 allow_no_calibration (7 帧)

T2 OIII/NGC1727 和 T3 NGC55 全部 6 帧在 CALIBRATE 阶段失败：

```
error_msg: "[CALIBRATE] 无 Master 文件且未启用 allow_no_calibration"
exit_code: 4
```

**根因**: stage1_config.json 中 `allow_no_calibration: false`，且未找到对应 T2 NGC1727 和 T3 NGC55 的 Master Bias/Dark/Flat 文件。

### 2.4 测光矩阵与诊断报告生成

生成以下报告文件：

- `reports/PHOTOMETRY_MATRIX.csv` (3420 bytes) — 16 帧测光矩阵
- `reports/photometric_diag_summary.json` (18671 bytes) — 测光诊断汇总（含 Gate 标准、分类统计、按滤镜类别统计、每帧详细 diag）
- `reports/failure_classification.json` (6726 bytes) — 失败帧分类

## 3. 测试文件

### 3.1 测试脚本

- **文件**: `engineering_v1.3/evidence/P12-004/scripts/run_photometric_matrix.py`
- **大小**: 26166 bytes
- **功能**: 16 帧代表帧批量测光校准 + 诊断收集 + Gate 检查 + 失败分类 + 报告生成

### 3.2 被测组件

- **orchestrator.exe**: `lib/orchestrator/cpp/orchestrator.exe`
- **photometric_calib.dll**: `lib/photometric_calib/cpp/photometric_calib.dll`
- **stage1 配置**: `lib/orchestrator/configs/stage1_config.json`
- **Gaia 数据**: `GaiaDR3SP/`
- **滤光片曲线**: `lib/photometric_calib/data/response_curves/filters.json`

### 3.3 测试数据

16 帧代表帧来自 P10-006 代表帧清单：

| 设备 | 滤镜 | 目标 | FITS 文件 |
|------|------|------|-----------|
| T4 | RED | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts |
| T4 | GREEN | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063620-180S-Green.fts |
| T4 | BLUE | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue.fts |
| T4 | HA | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@061318-300S-H-alpha.fts |
| T4 | OIII | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@063631-600S-Oiii.fts |
| T2 | RED | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts |
| T2 | GREEN | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts |
| T2 | BLUE | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts |
| T2 | HA | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts |
| T2 | OIII | NGC1727 | NGC1727_RGBHO_T2_flying_dutchman-20251031@075259-1800S-OIII.fts |
| T3 | RED | NGC55 | NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts |
| T3 | GREEN | NGC55 | NGC55_T3_flying_dutchman-20250701@075153-600S-Green.fts |
| T3 | BLUE | NGC55 | NGC55_T3_flying_dutchman-20250701@080333-600S-Blue.fts |
| T3 | HA | NGC55 | NGC55_T3_flying_dutchman-20250701@081412-1200S-H-alpha.fts |
| T3 | OIII | NGC55 | NGC55_T3_flying_dutchman-20250701@083458-1200S-Oiii.fts |
| T3 | LUM | NGC55 | NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts |

## 4. 关键指标汇总

| 指标 | 实际值 | Gate 标准 | 状态 |
|------|--------|-----------|------|
| 总帧数 | 16 | - | - |
| Gate PASS | 0 | ≥ 1 | FAIL |
| Gate FAIL | 16 | - | - |
| Gate 通过率 | 0.0% | - | FAIL |
| Broadband PASS | 0/10 | - | FAIL |
| Narrowband PASS | 0/6 | - | FAIL |
| INVALID_SCALE | 3 | - | - |
| STAGE1_ERROR | 13 | - | - |
| INSUFFICIENT_STARS | 0 | - | - |
| ZERO_SIGMA | 0 | - | - |
| TIMEOUT | 0 | - | - |

### 4.1 T4 RED/GREEN/BLUE 详细诊断（INVALID_SCALE）

| 字段 | T4 RED | T4 GREEN | T4 BLUE |
|------|--------|----------|---------|
| spectrum_rows_total | 0 | 0 | 0 |
| valid_fsyn | 0 | 0 | 0 |
| gaia_projected_in_frame | 6021 | 5892 | 6124 |
| psf_total | 2000 | 2000 | 2000 |
| psf_valid | 1984 | 1966 | 1922 |
| spatial_candidates | 1673 | 1623 | 1237 |
| unique_matches | 1673 | 1623 | 1237 |
| rejected_ambiguous | 0 | 0 | 0 |
| rejected_distance | 311 | 343 | 685 |
| rejected_quality | 3 | 4 | 6 |
| fit_used | 1670 | 1619 | 1231 |
| robust_iterations | 7 | 7 | 7 |
| scale_factor | 0.002836 | 0.002696 | 0.00261 |
| sigma_residual | 0.181595 | 0.157614 | 0.128533 |
| r_median | 2.531962 | 2.563027 | 2.569922 |
| r_p90 | 2.817502 | 2.7953 | 2.751839 |
| r_max | 3.35578 | 3.238333 | 3.181805 |
| match_distance_median | 0.3665 | 0.435792 | 0.957366 |
| match_distance_p90 | 0.649771 | 0.805548 | 1.730188 |
| match_distance_max | 1.941539 | 1.564568 | 1.99949 |
| n_matched | 1670 | 1619 | 1231 |

**关键观察**:
1. `spectrum_rows_total=0` 和 `valid_fsyn=0` 表明光谱合成阶段未产生有效数据，这是 scale_factor 异常的根因
2. 空间匹配工作正常（unique_matches=spatial_candidates，rejected_ambiguous=0），P12-002 修复的 KD-tree 工作正常
3. fit_used 数量充足（1231-1670），远超 Broadband Gate 阈值 20
4. sigma_residual 为正有限值，满足 Gate 要求
5. scale_factor ≈ 0.0026-0.0028 远低于 Gate 下限 0.01，表明测光定标零点异常

## 5. 失败原因汇总与后续

### 5.1 失败原因分类

| 失败类别 | 帧数 | 帧列表 | 根因 |
|----------|------|--------|------|
| INVALID_SCALE | 3 | T4 RED/GREEN/BLUE | scale_factor ≈ 0.0026-0.0028 超出 [0.01, 100.0]；valid_fsyn=0 表明光谱合成异常 |
| STAGE1_ERROR (滤光片曲线) | 2 | T4 HA/OIII | `[PHOTOMETRIC] 加载滤光片曲线失败`；map_filter_name 未映射窄带滤镜 |
| STAGE1_ERROR (中文路径) | 4 | T2 RED/GREEN/BLUE/HA (LDN43) | `filesystem error: Cannot convert character sequence`；C++ std::filesystem 无法处理中文路径 |
| STAGE1_ERROR (无 Master) | 7 | T2 OIII/NGC1727, T3 全部 6 帧 | `[CALIBRATE] 无 Master 文件且未启用 allow_no_calibration`；缺少 T2/T3 校准文件 |

### 5.2 后续任务

本任务确认 G12 Photometric Gate 未通过（0/16），需进入 P12-005 修复：

1. **修复窄带滤光片映射**: orchestrator 的 `map_filter_name` 需正确映射 "H-alpha" → 滤光片 ID，"OIII" → 滤光片 ID；`filters.json` 需补充窄带滤光片定义
2. **修复中文路径处理**: C++ 代码需使用宽字符 API 或 UTF-8 路径转换处理非 ASCII 路径
3. **补充 T2/T3 校准文件**: 提供 T2 NGC1727 和 T3 NGC55 的 Master Bias/Dark/Flat，或在配置中启用 `allow_no_calibration`
4. **调查 scale_factor 异常**: T4 RED/GREEN/BLUE 的 `valid_fsyn=0` 表明光谱合成阶段异常，需排查 F_syn 计算逻辑或滤光片/QE 曲线加载
5. **重新运行 16 帧测光矩阵**: 修复后重新执行本任务脚本验证 Gate 通过

## 6. 约束遵守

- ✅ 不修改测试代码或 DLL（仅运行和验证）
- ✅ 不修改 orchestrator 或 photometric_calib 源码
- ✅ 不执行 git commit/push（由主进程处理）
- ✅ 使用 PowerShell 7 环境
- ✅ 使用中文回复
- ✅ 单帧超时 600s
- ✅ 失败帧记录原因并继续其他帧
- ✅ 所有输出文件 UTF-8 编码
- ✅ 报告以中文为主，技术术语保留英文
- ✅ 参考 P12-002/P12-003 的证据文件结构

## 7. 关键证据文件

- 测光矩阵 CSV: `reports/PHOTOMETRY_MATRIX.csv`
- 测光诊断汇总: `reports/photometric_diag_summary.json`
- 失败帧分类: `reports/failure_classification.json`
- 测试脚本: `scripts/run_photometric_matrix.py`
- 主日志: `raw_logs/run_photometric_matrix_main.log`
- 16 帧原始日志: `raw_logs/<device>_<filter>_<target>/stage1.log`
- T4 RED/GREEN/BLUE photometry_report.json: `raw_logs/T4_*_Galaxy_Center/photometry_report.json`
- 任务报告: `TASK_REPORT.md` (本文件)
- 测试报告: `TEST_REPORT.md`
- 证据索引: `EVIDENCE_INDEX.md`
- 复核报告: `REVIEW_REPORT.md`
