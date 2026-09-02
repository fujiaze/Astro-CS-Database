# P12-004 测试报告

- **任务编号**: P12-004
- **测试日期**: 2026-07-28
- **测试环境**: Windows + PowerShell 7 + orchestrator.exe (骨架版本)
- **被测组件**: `lib/orchestrator/cpp/orchestrator.exe` + `lib/photometric_calib/cpp/photometric_calib.dll`
- **配置文件**: `lib/orchestrator/configs/stage1_config.json`
- **Gaia 数据**: `GaiaDR3SP/`
- **测试脚本**: `工程控制/evidence/P12-004/scripts/run_photometric_matrix.py`
- **测试结果**: `工程控制/evidence/P12-004/reports/PHOTOMETRY_MATRIX.csv`

## 1. 测试总览

**结果: 0/16 Gate PASS（全部失败）**

| 帧序 | 设备 | 滤镜 | 类别 | 目标 | exit_code | elapsed_s | fit_used | scale_factor | sigma_residual | Gate | 失败类别 |
|------|------|------|------|------|-----------|-----------|----------|--------------|----------------|------|----------|
| 1 | T4 | RED | Broadband | Galaxy_Center | 0 | 32.232 | 1670 | 0.002836 | 0.181595 | FAIL | INVALID_SCALE |
| 2 | T4 | GREEN | Broadband | Galaxy_Center | 0 | 35.477 | 1619 | 0.002696 | 0.157614 | FAIL | INVALID_SCALE |
| 3 | T4 | BLUE | Broadband | Galaxy_Center | 0 | 30.216 | 1231 | 0.00261 | 0.128533 | FAIL | INVALID_SCALE |
| 4 | T4 | HA | Narrowband | Galaxy_Center | 1 | 3.981 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 5 | T4 | OIII | Narrowband | Galaxy_Center | 1 | 4.077 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 6 | T2 | RED | Broadband | LDN43 | 3 | 3.703 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 7 | T2 | GREEN | Broadband | LDN43 | 3 | 4.172 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 8 | T2 | BLUE | Broadband | LDN43 | 3 | 50.919 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 9 | T2 | HA | Narrowband | LDN43 | 3 | 4.098 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 10 | T2 | OIII | Narrowband | NGC1727 | 4 | 0.226 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 11 | T3 | RED | Broadband | NGC55 | 4 | 0.21 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 12 | T3 | GREEN | Broadband | NGC55 | 4 | 0.223 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 13 | T3 | BLUE | Broadband | NGC55 | 4 | 0.22 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 14 | T3 | HA | Narrowband | NGC55 | 4 | 0.219 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 15 | T3 | OIII | Narrowband | NGC55 | 4 | 0.221 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |
| 16 | T3 | LUM | Broadband | NGC55 | 4 | 0.22 | 0 | 0.0 | 0.0 | FAIL | STAGE1_ERROR |

### 1.1 按滤镜类别统计

| 类别 | 总数 | PASS | FAIL | 通过率 |
|------|------|------|------|--------|
| Broadband (LUM/RED/GREEN/BLUE) | 10 | 0 | 10 | 0.0% |
| Narrowband (HA/OIII) | 6 | 0 | 6 | 0.0% |
| **总计** | **16** | **0** | **16** | **0.0%** |

### 1.2 按失败类别统计

| 失败类别 | 帧数 | 占比 |
|----------|------|------|
| INVALID_SCALE | 3 | 18.75% |
| STAGE1_ERROR | 13 | 81.25% |
| INSUFFICIENT_STARS | 0 | 0.0% |
| ZERO_SIGMA | 0 | 0.0% |
| TIMEOUT | 0 | 0.0% |

## 2. Gate 标准

| Gate 项 | Broadband 标准 | Narrowband 标准 | 验证结果 |
|---------|----------------|-----------------|----------|
| fit_used | ≥ 20 | ≥ 8 | 3 帧满足（T4 RED/GREEN/BLUE），13 帧因 stage1 失败 fit_used=0 |
| scale_factor | ∈ [0.01, 100.0] | ∈ [0.01, 100.0] | 0 帧满足（T4 RED/GREEN/BLUE 的 scale ≈ 0.0026-0.0028 超出下限） |
| sigma_residual | > 0 且有限 | > 0 且有限 | 3 帧满足（T4 RED/GREEN/BLUE），13 帧因 stage1 失败 sigma=0 |

## 3. 测试详情

### 3.1 T4 Galaxy_Center RED/GREEN/BLUE (INVALID_SCALE)

**测试条件**:
- 设备: T4 (Galaxy_Center, panel1)
- 滤镜: RED/GREEN/BLUE (Broadband)
- 曝光时间: 180s
- 图像尺寸: 4500×3600
- 校准文件: T4 calibration files（Master Bias/Dark/Flat 齐全）

**执行流程**:
- READ_FITS: 成功（0.17-0.19s）
- CALIBRATE: 成功（应用 Bias/Dark/Flat）
- PLATESOLVE: 成功（ipv_solver 求解 WCS）
- PSF: 成功（拟合 1922-1984 颗星）
- PHOTOMETRIC: 成功但 scale_factor 异常

**关键诊断**:
- 空间匹配工作正常：unique_matches=spatial_candidates，rejected_ambiguous=0（P12-002 修复的 KD-tree 工作正常）
- fit_used 充足：1231-1670（远超 Broadband Gate 阈值 20）
- sigma_residual 正有限：0.128-0.182（满足 Gate 要求）
- **scale_factor 异常**: 0.0026-0.0028（低于 Gate 下限 0.01）
- **valid_fsyn=0**: 光谱合成阶段未产生有效数据，可能是 scale_factor 异常的根因
- **spectrum_rows_total=0**: 光谱表为空

**T4 RED 完整诊断字段**:

| 字段 | 值 | 说明 |
|------|-----|------|
| spectrum_rows_total | 0 | 光谱表行数（异常：应为正数） |
| valid_fsyn | 0 | 有效 F_syn 数（异常：应为正数） |
| gaia_projected_in_frame | 6021 | 投影到画幅的 Gaia 星数 |
| psf_total | 2000 | PSF 检测总星数 |
| psf_valid | 1984 | PSF 有效星数 |
| spatial_candidates | 1673 | 空间候选匹配数 |
| unique_matches | 1673 | 双向唯一匹配数 |
| rejected_ambiguous | 0 | 拒绝（非互为最近邻） |
| rejected_distance | 311 | 拒绝（距离超限） |
| rejected_quality | 3 | 拒绝（质量不足） |
| fit_used | 1670 | 实际参与拟合的星对数 |
| robust_iterations | 7 | 稳健拟合迭代次数 |
| scale_factor | 0.002836 | 测光定标零点（异常：应 ∈ [0.01, 100.0]） |
| sigma_residual | 0.181595 | 残差 sigma |
| r_median | 2.531962 | 残差中位数 |
| r_p90 | 2.817502 | 残差 90 分位 |
| r_max | 3.35578 | 残差最大值 |
| match_distance_median | 0.3665 | 匹配距离中位数 |
| match_distance_p90 | 0.649771 | 匹配距离 90 分位 |
| match_distance_max | 1.941539 | 匹配距离最大值 |
| n_matched | 1670 | 匹配星对数 |

### 3.2 T4 Galaxy_Center HA/OIII (STAGE1_ERROR — 滤光片曲线加载失败)

**测试条件**:
- 设备: T4 (Galaxy_Center, panel1)
- 滤镜: H-alpha / OIII (Narrowband)
- 曝光时间: 300s (HA) / 600s (OIII)

**执行流程**:
- READ_FITS: 成功
- CALIBRATE: 成功（T4 校准文件齐全）
- PLATESOLVE: 成功
- PSF: 成功
- PHOTOMETRIC: **失败** — `[PHOTOMETRIC] 加载滤光片曲线失败`

**错误信息**:
```
exit_code: 1
error_msg: "[PHOTOMETRIC] 加载滤光片曲线失败"
```

**根因分析**:
- orchestrator 的 `map_filter_name` 函数未正确映射 "H-alpha" 和 "OIII" 到 `filters.json` 中的滤光片名称
- `filters.json` 中缺少窄带滤光片定义（仅含 38 种宽带/光污染滤镜，无 HA/OIII 窄带）
- 命令行传入 `--filter H-alpha` 和 `--filter OIII`，但 DLL 内部无法找到对应曲线

### 3.3 T2 LDN43 RED/GREEN/BLUE/HA (STAGE1_ERROR — 中文路径)

**测试条件**:
- 设备: T2 (LDN43)
- 滤镜: RED/GREEN/BLUE/HA
- 文件路径: `testdata\LDN43_T2素材_flying_dutchman\lights\...`（含中文 "素材"）

**执行流程**:
- READ_FITS: **失败** — `filesystem error: Cannot convert character sequence: Illegal byte sequence`

**错误信息**:
```
exit_code: 3
terminate called after throwing an instance of 'std::filesystem::__cxx11::filesystem_error'
  what():  filesystem error: Cannot convert character sequence: Illegal byte sequence
```

**根因分析**:
- C++ `std::filesystem` 在 Windows 下使用系统本地编码（GBK/CP936）处理路径
- 路径中包含中文字符 "素材"，PowerShell 传入的 UTF-8 编码路径无法被 `std::filesystem` 正确转换
- T2 BLUE 帧耗时 50.9s（其他 3 帧仅 3.7-4.2s），可能是部分文件操作成功后才在后续阶段失败

### 3.4 T2 OIII/NGC1727 + T3 NGC55 全部 (STAGE1_ERROR — 无 Master 文件)

**测试条件**:
- 设备: T2 (NGC1727), T3 (NGC55)
- 滤镜: OIII/RED/GREEN/BLUE/HA/LUM
- stage1_config.json: `allow_no_calibration: false`

**执行流程**:
- READ_FITS: 成功
- CALIBRATE: **失败** — `[CALIBRATE] 无 Master 文件且未启用 allow_no_calibration`

**错误信息**:
```
exit_code: 4
error_msg: "[CALIBRATE] 无 Master 文件且未启用 allow_no_calibration"
```

**根因分析**:
- stage1_config.json 中 `calibration_dir: "testdata/T4 calibration files"`，仅含 T4 校准文件
- T2 NGC1727 和 T3 NGC55 的 Master Bias/Dark/Flat 文件不在该目录
- `allow_no_calibration: false` 导致缺少 Master 时直接失败，不降级处理

## 4. 测试结论

### 4.1 测试结果

- **0/16 Gate PASS**: 全部 16 帧 Gate 失败
- **3 帧 stage1 成功但 Gate 失败**: T4 RED/GREEN/BLUE（INVALID_SCALE）
- **13 帧 stage1 失败**: 2 帧（滤光片曲线加载失败）+ 4 帧（中文路径）+ 7 帧（无 Master 文件）

### 4.2 测试覆盖

| 维度 | 覆盖 | 说明 |
|------|------|------|
| 设备覆盖 | ✅ | T2/T3/T4 三种设备 |
| 滤镜覆盖 | ✅ | LUM/RED/GREEN/BLUE/HA/OIII 六种滤镜 |
| 目标覆盖 | ✅ | Galaxy_Center/LDN43/NGC1727/NGC55 四个目标 |
| 滤镜类别覆盖 | ✅ | Broadband (10 帧) + Narrowband (6 帧) |
| 诊断字段覆盖 | ✅ | 20 字段完整收集（成功帧）+ 部分收集（失败帧） |

### 4.3 P12-002 修复验证

本任务间接验证了 P12-002 修复的有效性：

- T4 RED/GREEN/BLUE 三帧 stage1 成功执行到 PHOTOMETRIC 阶段
- 空间匹配工作正常：unique_matches=spatial_candidates，rejected_ambiguous=0
- fit_used 充足（1231-1670），表明 KD-tree 方向 bug 修复和双向最近邻唯一配对工作正常
- P12-002 修复未引入回归（空间匹配层面）

### 4.4 发现的问题

本任务发现 4 类问题，需在 P12-005 修复：

1. **scale_factor 异常** (3 帧): T4 RED/GREEN/BLUE 的 scale_factor ≈ 0.0026-0.0028，valid_fsyn=0，需调查光谱合成逻辑
2. **窄带滤光片缺失** (2 帧): filters.json 缺少 HA/OIII 定义，map_filter_name 未正确映射
3. **中文路径处理** (4 帧): C++ std::filesystem 无法处理非 ASCII 路径
4. **校准文件缺失** (7 帧): T2/T3 缺少 Master 文件，配置未启用 allow_no_calibration

## 5. 测试环境

| 项目 | 值 |
|------|-----|
| 操作系统 | Windows |
| Shell | PowerShell 7 |
| orchestrator 版本 | 骨架版本 |
| orchestrator 路径 | `lib/orchestrator/cpp/orchestrator.exe` |
| photometric_calib.dll | `lib/photometric_calib/cpp/photometric_calib.dll` |
| stage1 配置 | `lib/orchestrator/configs/stage1_config.json` |
| Gaia 数据目录 | `GaiaDR3SP/` |
| 校准文件目录 | `testdata/T4 calibration files/` |
| 滤光片曲线 | `lib/photometric_calib/data/response_curves/filters.json` |
| QE 曲线 | `lib/photometric_calib/data/response_curves/qe_curves.json` |
| 单帧超时 | 600s |
| 日志级别 | INFO |
| 可用线程数 | 16 |
