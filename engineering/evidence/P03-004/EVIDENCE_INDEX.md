# P03-004 证据索引

## 任务信息
- **任务 ID**: P03-004
- **任务名称**: SNR 稀疏模型与 SIP 一致性 (v1.1 开发包)
- **执行日期**: 2026-07-25
- **VERDICT**: PASS

## 证据清单

### 1. 验证脚本
- **文件**: `engineering/evidence/P03-004/verify_snr_model.py`
- **描述**: P03-004 SNR 稀疏模型与 SIP 一致性验证脚本 (5 个测试用例)
- **覆盖**:
  - A. snr_model schema 验证
  - B. WCS+SIP 转球面与 astropy all_pix2world 一致性
  - C. SIP 修正在边缘像素生效
  - D. 退化路径返回码 (n_stars=0, sigma=0, nullptr)
  - E. HISS 稀疏模型 (snr_format=1) 写入/读取往返

### 2. 测试日志
- **文件**: `engineering/evidence/P03-004/verify_snr_model.log`
- **描述**: verify_snr_model.py 完整输出日志
- **关键结果**: 5/5 PASS

### 3. 验证结果 JSON
- **文件**: `engineering/evidence/P03-004/snr_model_validation.json`
- **描述**: 结构化验证结果 (机器可读)
- **字段**: task_id, verdict, summary, tests[], stage1_integration_test, code_changes[], constraints_verified[]

### 4. 任务报告
- **文件**: `engineering/evidence/P03-004/TASK_REPORT.md`
- **描述**: 任务执行报告 (目标, 实现, 代码变更, 兼容性)
- **VERDICT**: PASS

### 5. 测试报告
- **文件**: `engineering/evidence/P03-004/TEST_REPORT.md`
- **描述**: 详细测试报告 (单元测试 + 集成测试, 含数据表)
- **结果**: 5/5 单元测试 PASS, 集成测试 PASS

### 6. 复核报告
- **文件**: `engineering/evidence/P03-004/REVIEW_REPORT.md`
- **描述**: 代码复核报告 (检查项 + 复核结论)
- **VERDICT**: PASS

### 7. Stage1 集成测试输出
- **文件**: `engineering/evidence/P03-004/test_normal.hiss`
- **描述**: stage1 集成测试输出 HISS 文件 (3928 HEALPix 像素, has_snr=0)
- **说明**: SNR stage 因 sigma_residual=0 降级, has_snr=0 符合预期

## 代码变更证据

### 修改文件
1. `lib/snr_estimator/cpp/include/snr_estimator.h`
   - 新增 SnrSipCoeffs 结构 (lines 45-66)
   - 新增 SNR_SIP_MAX_ORDER=5, SNR_SIP_COEFF_SIZE=36 宏

2. `lib/snr_estimator/cpp/src/snr_estimator.cpp`
   - 新增 snrEvalSip() 前向 SIP 多项式求值 (lines 213-231)
   - 新增 pixelToSkySimple() 完整 WCS+SIP TAN 反投影 (lines 244-306)
   - snr_extract_model() 使用 pixelToSkySimple (line 408)

3. `lib/orchestrator/cpp/src/orchestrator.cpp`
   - run_stage_snr 新增 SIP 系数读取 (lines 2818-2865)
   - 新增日志: "[SNR] SIP 前向系数加载: A_ORDER=X B_ORDER=X"

### 构建产物
1. `lib/snr_estimator/cpp/snr_estimator.dll` (951.1 KB, 2026-07-25 编译)
2. `build/artifacts/snr_estimator.dll` (已部署, 973907 字节)
3. `lib/orchestrator/cpp/orchestrator.exe` (2026-07-25 编译)

## 关键指标

| 指标 | 值 | 阈值 | 余量 |
|------|-----|------|------|
| WCS+SIP max\|Δra\| | 5.684e-14 度 | 1e-9 度 | 5 个数量级 |
| WCS+SIP max\|Δdec\| | 1.243e-14 度 | 1e-9 度 | 5 个数量级 |
| SIP 边缘修正量 | 3.377e-6 度 | 1e-7 度 | 1.3 个数量级 |
| HISS 往返 snr_phot 偏差 | <1e-9 | 1e-9 | 达标 |
| HISS 往返 cp.ra 偏差 | <1e-9 | 1e-9 | 达标 |
| 单元测试通过率 | 5/5 (100%) | 5/5 | 达标 |
| 集成测试 | success=true | success=true | 达标 |
